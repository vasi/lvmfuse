#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <openssl/aes.h>
#include <openssl/sha.h>

using std::string;
using std::cin;
using std::cout;
using std::ifstream;

typedef std::vector<uint8_t> buf_t;

const size_t Sector = 512;

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
	for (buf_t::const_iterator i = buf.begin(); i != buf.end(); ++i) {
		s.push_back(hexchar(*i >> 4));
		s.push_back(hexchar(*i & 0xF));
	}
	return s;
}

uint64_t parse_int(const char *p) {
	return strtoll(p, NULL, 0);
}

buf_t read_stream(std::istream& is) {
	buf_t r;
	while (true) {
		size_t off = r.size();
		r.resize(off + Sector);
		is.read((char*)&r[off], Sector);
		size_t count = is.gcount();
		if (count < Sector) {
			r.resize(off + count);
			return r;
		}
	}
}

void store64le(uint64_t v, uint8_t *p) {
	for (; v > 0; v >>= 8)
		*p++ = v & 0xff;
}

void aes_cbc_essiv_sha256_dec(const buf_t& key, const buf_t& ct, buf_t& pt,
		uint64_t sector) {
	// Hash the key to get a salt
	buf_t salt(SHA256_DIGEST_LENGTH);
	SHA256(&key[0], key.size(), &salt[0]);
	
	// Encrypt the sector number using the salt as key
	buf_t secbuf(AES_BLOCK_SIZE);
	store64le(sector, &secbuf[0]);
	AES_KEY ivkey;
	AES_set_encrypt_key(&salt[0], salt.size() * 8, &ivkey);
	buf_t iv(AES_BLOCK_SIZE);
	AES_encrypt(&secbuf[0], &iv[0], &ivkey);
	
	// Use the encrypted sector number as IV
	pt.resize(ct.size());
	AES_KEY blockkey;
	AES_set_decrypt_key(&key[0], key.size() * 8, &blockkey);
	AES_cbc_encrypt(&ct[0], &pt[0], ct.size(), &blockkey, &iv[0], false);
}

void bufxor(buf_t& dest, const buf_t& arg) {
	for (size_t i = 0; i < dest.size() && i < arg.size(); ++i)
		dest[i] ^= arg[i];
}

void tweak_mult(buf_t& t) {
	int i = t.size() - 1;
	bool carry = !!(t[i] & 0x80);
	for (; i > 0; --i)
		t[i] = (t[i] << 1) + (t[i-1] >> 7);
	t[0] = (t[0] << 1) ^ (0x87 * carry);
}

void xex(buf_t& dst, const uint8_t *src, const AES_KEY *key,
		const buf_t& tweak) {
	memcpy(&dst[0], src, AES_BLOCK_SIZE);
	bufxor(dst, tweak);
	AES_decrypt(&dst[0], &dst[0], key);
	bufxor(dst, tweak);
}

void aes_xts_plain64_dec(const buf_t& key, const buf_t& ct, buf_t& pt,
		uint64_t sector) {
	size_t klen = key.size() / 2;
	buf_t k1(&key[0], &key[klen]), k2(&key[klen], &key[key.size()]);
	size_t len = ct.size(), mod = len % AES_BLOCK_SIZE;
	
	// Initialize the tweak
	buf_t tweak(AES_BLOCK_SIZE);
	store64le(sector, &tweak[0]);
	AES_KEY tweakkey;
	AES_set_encrypt_key(&k2[0], k2.size() * 8, &tweakkey);
	AES_encrypt(&tweak[0], &tweak[0], &tweakkey);
	
	// Decrypt each block
	AES_KEY blockkey;
	AES_set_decrypt_key(&k1[0], k1.size() * 8, &blockkey);
	buf_t block(AES_BLOCK_SIZE);
	pt.resize(len);
	size_t end = mod ? len - mod - AES_BLOCK_SIZE : len;
	for (size_t i = 0; i < end; i += AES_BLOCK_SIZE) {
		xex(block, &ct[i], &blockkey, tweak);
		memcpy(&pt[i], &block[0], AES_BLOCK_SIZE);
		tweak_mult(tweak);
	}
	if (!mod)
		return;
	
	// Cipertext returning
	buf_t tweak2(tweak);
	tweak_mult(tweak2);
	xex(block, &ct[end], &blockkey, tweak2);
	memcpy(&pt[end + AES_BLOCK_SIZE], &block[0], mod);
	memcpy(&block[0], &ct[end + AES_BLOCK_SIZE], mod);
	xex(block, &block[0], &blockkey, tweak);
	memcpy(&pt[end], &block[0], AES_BLOCK_SIZE);
}

int main(int argc, char *argv[]) {
	buf_t key(hex2bin(argv[1]));
	uint64_t sector(parse_int(argv[2]));
//	ifstream infile("block.dd");
//	buf_t input(read_stream(infile));
	buf_t input(hex2bin(argv[3]));
	
	buf_t output;
	aes_xts_plain64_dec(key, input, output, sector);
	
	cout << bin2hex(output) << "\n";
	return 0;
}
