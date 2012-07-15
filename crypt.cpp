#include <tomcrypt.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <boost/lexical_cast.hpp>

using std::string;
using std::vector;
using boost::lexical_cast;

uint8_t hexval(char c) {
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'z')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'Z')
		return c - 'A' + 10;
	return 0;
}

void aes_xts_plain64(const string& key, uint64_t sector,
		uint8_t *ct, uint8_t *pt) {
	size_t klen = key.size() / 2;
	uint8_t tweak[16];
	memset(tweak, 0, 16);
	STORE64L(sector, tweak);
	
	register_cipher(&aes_desc);
	symmetric_xts xts;
	xts_start(find_cipher("aes"),
		(uint8_t*)&key[0], (uint8_t*)&key[klen], klen, 0, &xts);
	xts_decrypt(ct, 512, pt, tweak, &xts);
}

void aes_cbc_essiv_sha256(const string& key, uint64_t sector,
		uint8_t *ct, uint8_t *pt) {
	hash_state hf;
	sha256_init(&hf);
	sha256_process(&hf, (uint8_t*)&key[0], key.size());
	uint8_t salt[32];
	sha256_done(&hf, salt);
	
	uint8_t secbuf[16];
	memset(secbuf, 0, 16);
	STORE64L(sector, secbuf);
	
	symmetric_key ivkey;
	aes_setup(salt, 32, 0, &ivkey);
	uint8_t iv[16];
	aes_ecb_encrypt(secbuf, iv, &ivkey);
	
	register_cipher(&aes_desc);
	symmetric_CBC cbc;
	cbc_start(find_cipher("aes"), iv, (uint8_t*)&key[0], key.size(), 0, &cbc);
	cbc_decrypt(ct, pt, 512, &cbc);
}

string slurp(const char *file) {
	std::ifstream input(file);
	std::stringstream ss;
	ss << input.rdbuf();
	return ss.str();
}

int main(int argc, char *argv[]) {
	char *keyhex = argv[1];
	size_t hexlen = strlen(keyhex);
	string key;
	for (char *c = keyhex; c < keyhex + hexlen; ) {
		uint8_t b = hexval(*c++) << 4;
		key.push_back(b + hexval(*c++));
	}
	
	uint64_t sector = lexical_cast<uint64_t>(argv[2]);
	
	string str(slurp("block.dd"));
	vector<uint8_t> ct(str.begin(), str.end());
	vector<uint8_t> pt(str.size());
	
	aes_xts_plain64(key, sector, &ct[0], &pt[0]);
	
	fwrite(&pt[0], pt.size(), 1, stdout);
	return 0;
}
