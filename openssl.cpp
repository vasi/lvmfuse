#include <iostream>
#include <string>
#include <vector>

#include <boost/lexical_cast.hpp>

#include <openssl/aes.h>
#include <openssl/sha.h>

using std::string;
using std::cin;
using std::cout;
using boost::lexical_cast;

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

buf_t read_stdin() {
	buf_t r;
	while (true) {
		size_t off = r.size();
		r.resize(off + Sector);
		cin.read((char*)&r[off], Sector);
		size_t count = cin.gcount();
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


int main(int argc, char *argv[]) {
	buf_t key(hex2bin(argv[1]));
	off_t sector(lexical_cast<off_t>(argv[2]));
	buf_t input(read_stdin());
	
	buf_t output;
	aes_cbc_essiv_sha256_dec(key, input, output, sector);
	
	cout.write((char*)&output[0], output.size());
	return 0;
}
