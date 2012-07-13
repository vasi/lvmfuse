#include <iostream>
#include <string>
#include <vector>
#include <fcntl.h>
#include <sys/stat.h>
#include <termios.h>

#include <boost/lexical_cast.hpp>
#include <tomcrypt.h>

using std::string;
using std::cout;
using std::cerr;
typedef std::vector<uint8_t> buf_t;
typedef buf_t::iterator buf_i;
typedef buf_t::const_iterator buf_ci;
using boost::lexical_cast;

const off_t Sector = 512;
const char LuksMagic[] = {'L','U','K','S', 0xba, 0xbe};
const size_t LuksKeySlots = 8;
const uint32_t LuksKeyActive = 0x00AC71F3;

struct luks_keyslot {
	uint32_t active;
	uint32_t iterations;
	uint8_t salt[32];
	uint32_t km_offset; // sectors
	uint32_t stripes;
} __attribute__((packed));

struct luks_header {
	char magic[6];
	uint16_t version;
	char cipher_name[32];
	char cipher_mode[32];
	char hash_spec[32];
	uint32_t payload_offset; // sectors
	uint32_t key_bytes;
	uint8_t mk_digest[20];
	uint8_t mk_digest_salt[32];
	uint32_t mk_digest_iter;
	char uuid[40];
	luks_keyslot keyslots[LuksKeySlots];
} __attribute__((packed));

void die(string msg) {
	cerr << msg << "\n";
	exit(-1);
}

template <typename T>
void swap_be(T& val) {
	T res;
	uint8_t *byte = reinterpret_cast<uint8_t*>(&val);
	for (int i = 0; i < sizeof(T); ++i) {
		res <<= 8;
		res += byte[i];
	}
	val = res;
}

uint8_t hexval(char c) {
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'z')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'Z')
		return c - 'A' + 10;
	return 0;
}

buf_t hex2bin(const char *start, const char *end) {
	buf_t r;
	uint8_t b;
	while (start < end) {
		b = hexval(*start++) << 4;
		r.push_back(b + hexval(*start++));
	}
	return r;
}

buf_t hex2bin(const string& s) {
	const char *c = s.c_str();
	return hex2bin(c, c + s.size());
}

char hexchar(uint8_t v) {
	if (v < 10)
		return '0' + v;
	return 'a' + (v - 10);
}

string bin2hex(const buf_t& buf) {
	string s;
	for (buf_ci i = buf.begin(); i != buf.end(); ++i) {
		s.push_back(hexchar(*i >> 4));
		s.push_back(hexchar(*i & 0xF));
	}
	return s;
}

void register_algorithms() {
	register_hash(&sha1_desc);
	register_hash(&sha256_desc);
	register_cipher(&aes_desc);
}

buf_t pbkdf2(const buf_t& pass, const buf_t& salt,
		size_t iters, size_t outlen) {
	int hash = find_hash("sha1");
	buf_t out(outlen);
	pkcs_5_alg2(&pass[0], pass.size(), &salt[0], salt.size(), iters,
		hash, &out[0], &outlen);
	out.resize(outlen);
	return out;
}

void aes_xts_plain64_dec(const buf_t& key, const buf_t& ct, buf_t& pt,
		uint64_t sector) {
	size_t klen = key.size() / 2;
	
	buf_t tweak(16);
	STORE64L(sector, &tweak[0]);
	
	symmetric_xts xts;
	xts_start(find_cipher("aes"), &key[0], &key[klen], klen, 0, &xts);
	pt.resize(ct.size());
	xts_decrypt(&ct[0], ct.size(), &pt[0], &tweak[0], &xts);
}

void aes_cbc_essiv_sha256_dec(const buf_t& key, const buf_t& ct, buf_t& pt,
		uint64_t sector) {
	int hash = find_hash("sha256");
	int cipher = find_cipher("aes");
	
	buf_t salt(hash_descriptor[hash].hashsize);
	size_t ssize = salt.size();
	hash_memory(hash, &key[0], key.size(), &salt[0], &ssize);
	
	buf_t secbuf(cipher_descriptor[cipher].block_length);
	STORE64L(sector, &secbuf[0]);
	
	symmetric_key ivkey;
	aes_setup(&salt[0], salt.size(), 0, &ivkey);
	buf_t iv(secbuf.size());
	aes_ecb_encrypt(&secbuf[0], &iv[0], &ivkey);
	
	symmetric_CBC cbc;
	cbc_start(cipher, &iv[0], &key[0], key.size(), 0, &cbc);
	pt.resize(ct.size());
	cbc_decrypt(&ct[0], &pt[0], ct.size(), &cbc);
}

typedef void (*decryptor)(const buf_t& key, const buf_t& ct, buf_t& pt,
		uint64_t sector);

decryptor get_decryptor(const luks_header& hdr) {
	buf_t namebuf(hdr.cipher_name, hdr.cipher_name + sizeof(hdr.cipher_name));
	buf_t modebuf(hdr.cipher_mode, hdr.cipher_mode + sizeof(hdr.cipher_mode));
	namebuf.back() = 0;
	modebuf.back() = 0;
	string name((char*)&namebuf[0]);
	string mode((char*)&modebuf[0]);
	
	string combined = name + "-" + mode;
	if (combined == "aes-cbc-essiv:sha256")
		return aes_cbc_essiv_sha256_dec;
	else if (combined == "aes-xts-plain64")
		return aes_xts_plain64_dec;
	die("Unknown decryptor " + combined);
	return NULL;
}

// Assume divisible by Sector!
void decrypt_split(decryptor dec, const buf_t& key, buf_t& buf) {
	buf_t b(Sector);
	for (size_t i = 0; i < buf.size() / Sector; ++i) {
		size_t off = i * Sector;
		memcpy(&b[0], &buf[off], Sector);
		dec(key, b, b, i);
		memcpy(&buf[off], &b[0], Sector);
	}
}

buf_t hash_ext1(const buf_t& input, int hash) {
	size_t hsz = hash_descriptor[hash].hashsize;
	buf_t ret;
	
	hash_state hs;
	for (uint32_t i = 0; i * hsz <= input.size(); ++i) {
		hash_descriptor[hash].init(&hs);
		
		uint8_t seg[sizeof(uint32_t)];
		STORE32H(i, seg);
		hash_descriptor[hash].process(&hs, seg, sizeof(seg));
		
		size_t off = i * hsz, sz = input.size() - off;
		if (sz > hsz)
			sz = hsz;
		hash_descriptor[hash].process(&hs, &input[off], sz);
		
		ret.resize(ret.size() + hsz);
		hash_descriptor[hash].done(&hs, &ret[off]);
		if (sz < hsz)
			ret.resize(ret.size() + sz - hsz);
	}
	return ret;
}

void bufxor(buf_t& a, const buf_t& b) {
	for (size_t i = 0; i < a.size() && i < b.size(); ++i)
		a[i] = a[i] ^ b[i];
}

buf_t afmerge(const buf_t& input, size_t size, size_t stripes, int hash) {
	buf_t d(size);
	for (size_t n = 0; n < stripes; ++n) {
		buf_t s(&input[n * size], &input[(n + 1) * size]);
		bufxor(d, s);
		if (n < stripes - 1)
			d = hash_ext1(d, hash);
	}
	return d;
}

void luks_open_header(luks_header& hdr) {
	swap_be(hdr.version);
	swap_be(hdr.payload_offset);
	swap_be(hdr.key_bytes);
	swap_be(hdr.mk_digest_iter);
	for (size_t i = 0; i < LuksKeySlots; ++i) {
		luks_keyslot *s = &hdr.keyslots[i];
		swap_be(s->active);
		swap_be(s->iterations);
		swap_be(s->km_offset);
		swap_be(s->stripes);
	}
	
	if (strncmp(hdr.magic, LuksMagic, sizeof(LuksMagic)) != 0)
		die("Bad LUKS magic");
	if (hdr.version != 1)
		die("Bad LUKS version");
}

int luks_open(const char *file, luks_header& hdr) {
	int fd = open(file, O_RDONLY);
	read(fd, &hdr, sizeof(hdr));
	luks_open_header(hdr);
	return fd;
}

string readpass(const char *prompt) {
	cerr << prompt << ": ";
	struct termios told, tnew;
	tcgetattr(STDIN_FILENO, &told);
	tnew = told;
	tnew.c_lflag = (tnew.c_lflag & ~ECHO) | ECHONL;
	tcsetattr(STDIN_FILENO, TCSANOW, &tnew);
	
	string r;
	getline(std::cin, r);
	
	tcsetattr(STDIN_FILENO, TCSANOW, &told);
	return r;
}

bool checkpass1(int fd, const luks_header& header, size_t keyslot_idx,
		const buf_t& password, buf_t& key) {
	luks_keyslot keyslot(header.keyslots[keyslot_idx]);
	if (keyslot.active != LuksKeyActive)
		return false;
	
	// Generate the password key from the password
	buf_t keyslot_salt(keyslot.salt, keyslot.salt + sizeof(keyslot.salt));
	buf_t password_key(pbkdf2(password, keyslot_salt, keyslot.iterations,
		header.key_bytes));
	
	// Read the key material
	buf_t key_material(header.key_bytes * keyslot.stripes);
	lseek(fd, keyslot.km_offset * Sector, SEEK_SET);
	read(fd, &key_material[0], key_material.size());
	
	// Generate the master key candidate
	decryptor dec = get_decryptor(header);
	decrypt_split(dec, password_key, key_material);
	buf_t master(afmerge(key_material, header.key_bytes, keyslot.stripes,
		find_hash("sha1")));
	
	// Generate the master key digest
	buf_t digest_salt(header.mk_digest_salt,
		header.mk_digest_salt + sizeof(header.mk_digest_salt));
	buf_t digest_candidate(pbkdf2(master, digest_salt, header.mk_digest_iter,
		sizeof(header.mk_digest)));
	
	// Check for a match
	buf_t digest(header.mk_digest,
		header.mk_digest + sizeof(header.mk_digest));
	if (digest == digest_candidate) {
		key = master;
		return true;
	}
	return false;
}

bool checkpass(int fd, const luks_header& header, const buf_t& password,
		buf_t& key) {
	for (size_t slot = 0; slot < LuksKeySlots; ++slot) {
		if (checkpass1(fd, header, slot, password, key))
			return true;
	}
	return false;
}

int main(int argc, char *argv[]) {
	register_algorithms();
	
	luks_header hdr;
	int fd = luks_open(argv[1], hdr);
	
	buf_t password;
	if (argc >= 3) {
		const char *passarg = argv[2];
		struct stat st;
		if (stat(passarg, &st) == -1) { // Use the arg
			password.assign(passarg, passarg + strlen(passarg));
		} else {						// Use the file contents
			password.resize(st.st_size);
			int pwfd = open(passarg, O_RDONLY);
			read(pwfd, &password[0], password.size());
			close(pwfd);
		}
	} else {
		string pwstr(readpass("Password"));
		password.assign(pwstr.begin(), pwstr.end());
	}
	
	buf_t key;
	if (checkpass(fd, hdr, password, key)) {
		cout << bin2hex(key) << "\n";
	} else {
		cerr << "Not a valid password\n";
	}
	return 0;
}
