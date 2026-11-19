/*
Copyright (C) 2018 Jief Luce
Copyright (C) 2017 Simon Gander

This file is originally based on the vfdecrypt sources.
This file is also based on the work of Simon Gander (https://github.com/sgan81/apfs-fuse)

You should have received a copy of the GNU General Public License
along with hdimount.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "EncryptReader.h"

#include <cstring>
#include <cassert>
#include <vector>
#include <iostream>
#include <iomanip>

#include <stdio.h>
#include <unistd.h>
#include <inttypes.h> // for strtoumax
#include <fcntl.h> // open, read, close...
#include <arpa/inet.h>
#include <openssl/sha.h>
#include <openssl/aes.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#if defined(__linux__) || defined(__APPLE__)
#include <termios.h>
#endif

#include "exceptions.h"
#include "be.h"


/* length of message digest output in bytes (160 bits) */
#define MD_LENGTH		20
/* block size of cipher in bytes (128 bits) */
#define CIPHER_BLOCKSIZE	16


#pragma pack(push, 1)

struct DmgCryptHeaderV2
{
	char signature[8];
	uint32_t maybe_version;
	uint32_t enc_iv_size;
	uint32_t encMode;
	uint32_t encAlg;
	uint32_t key_bits;
	uint32_t prngalg;
	uint32_t prngkeysize;
	uint8_t uuid[0x10];
	uint32_t block_size;
	uint64_t encrypted_data_length;
	uint64_t encrypted_data_offset;
	uint32_t no_of_keys;
};

struct DmgKeyPointer
{
	uint32_t key_type;
	uint64_t key_offset;
	uint64_t key_length;
};

struct DmgKeyData
{
	uint32_t kdf_algorithm;
	uint32_t prng_algorithm;
	uint32_t iteration_count;
	uint32_t salt_len;
	uint8_t salt[0x20];
	uint32_t blob_enc_iv_size;
	uint8_t blob_enc_iv[0x20];
	uint32_t blob_enc_key_bits;
	uint32_t blob_enc_algorithm;
	uint32_t blob_enc_padding;
	uint32_t blob_enc_mode;
	uint32_t encr_key_blob_size;
	uint8_t encr_key_blob[0x200];
};

#pragma pack(pop)

bool EncryptReader::isEncrypted(std::shared_ptr<Reader> reader)
{
	char data[0x1000];
	std::vector<uint8_t> kdata;

	const DmgCryptHeaderV2 *hdr;
	const DmgKeyPointer *keyptr;
	const DmgKeyData *keydata;
	uint32_t no_of_keys;
	uint32_t key_id;

	reader->read(data, sizeof(data), 0);

	assert(sizeof(data) > sizeof(DmgCryptHeaderV2));
	hdr = reinterpret_cast<const DmgCryptHeaderV2 *>(data);

	return (memcmp(hdr->signature, "encrcdsa", 8) == 0);
}

bool EncryptReader::SetupEncryptionV2(const char* password)
{
	char data[0x1000];
	std::vector<uint8_t> kdata;

	const DmgCryptHeaderV2 *hdr;
	const DmgKeyPointer *keyptr;
	const DmgKeyData *keydata;
	uint32_t no_of_keys;
	uint32_t key_id;

	m_reader->read(data, sizeof(data), 0);

	assert(sizeof(data) > sizeof(DmgCryptHeaderV2));
	hdr = reinterpret_cast<const DmgCryptHeaderV2 *>(data);

	if (memcmp(hdr->signature, "encrcdsa", 8))
		return false;

	m_crypt_offset = be(hdr->encrypted_data_offset);
	m_crypt_size = be(hdr->encrypted_data_length);
	if (be(hdr->block_size) > INT_MAX)
        throw io_error(std::string("Encrypted block sizeis too big : ") + std::to_string(be(hdr->block_size)) + ". Usually 512 or 4096");
	m_crypt_blocksize = be((int32_t)hdr->block_size);
	
	no_of_keys = be(hdr->no_of_keys);

	for (key_id = 0; key_id < no_of_keys; key_id++)
	{
		assert(sizeof(data) > sizeof(DmgCryptHeaderV2) + key_id * sizeof(DmgKeyPointer) + sizeof(DmgKeyPointer));
		keyptr = reinterpret_cast<const DmgKeyPointer *>(data + sizeof(DmgCryptHeaderV2) + key_id * sizeof(DmgKeyPointer));

		if (be(keyptr->key_length) > INT32_MAX)
            throw io_error(std::string("Key too big (") + std::to_string(be(keyptr->key_length)) + " bytes)");

		kdata.resize(be(keyptr->key_length));
		int32_t bytes_read = m_reader->read(reinterpret_cast<char *>(kdata.data()), kdata.size(), be(keyptr->key_offset));
		if ( bytes_read != (int32_t)kdata.size())
            throw io_error(std::string("Cannot read ") + std::to_string(kdata.size()) + " bytes at offset "+std::to_string(be(keyptr->key_offset))+". Returns " + std::to_string(bytes_read));

		keydata = reinterpret_cast<const DmgKeyData *>(kdata.data());

		uint8_t derived_key[192/8];

		PKCS5_PBKDF2_HMAC_SHA1(password, strlen(password), (unsigned char*)keydata->salt, be(keydata->salt_len), be(keydata->iteration_count), sizeof(derived_key), derived_key);

		uint32_t blob_len = be(keydata->encr_key_blob_size);
		uint8_t blob[blob_len]; // /* result of the decryption operation shouldn't be bigger than ciphertext */

		EVP_CIPHER_CTX ctx;
		int outlen, tmplen;

		EVP_CIPHER_CTX_init(&ctx);
		EVP_DecryptInit_ex(&ctx, EVP_des_ede3_cbc(), NULL, derived_key, keydata->blob_enc_iv);
		if(!EVP_DecryptUpdate(&ctx, blob, &outlen, keydata->encr_key_blob, blob_len)) {
			throw io_error("internal error (1) during key unwrap operation!");
		}
		if(!EVP_DecryptFinal_ex(&ctx, blob + outlen, &tmplen)) {
			throw io_error("internal error (2) during key unwrap operation! Wrong password ?");
		}
		//outlen += tmplen;
		EVP_CIPHER_CTX_cleanup(&ctx);

		uint8_t aes_key[32]; // up to aes 256 bits
		memcpy((void*)&aes_key, blob, be(hdr->key_bits)/8);
		memcpy(m_hmacsha1_key, blob+be(hdr->key_bits)/8, HMACSHA1_KEY_SIZE);
		AES_set_decrypt_key(aes_key, be(hdr->key_bits), &m_aes_decrypt_key);

		if (blob[blob_len - 1] < 1 || blob[blob_len - 1] > 8)
			continue;
		blob_len -= blob[blob_len - 1];

		if (memcmp(blob + blob_len - 5, "CKIE", 4))
			continue;

		return true;
	}

	return false;
}

EncryptReader::EncryptReader(std::shared_ptr<Reader> reader, const char* password)
: m_reader(reader)
{
	
	if (!SetupEncryptionV2(password))
	{
		throw io_error("Not encrypted");
	}
}

EncryptReader::~EncryptReader()
{
}

void EncryptReader::compute_iv(uint32_t blkid, uint8_t *iv)
{
	blkid = be(blkid);
	unsigned char mdResultOpenSsl[MD_LENGTH];
	unsigned int mdLenOpenSsl;
	HMAC(EVP_sha1(), m_hmacsha1_key, sizeof(m_hmacsha1_key), (const unsigned char *) &blkid, sizeof(blkid), mdResultOpenSsl, &mdLenOpenSsl);
	memcpy(iv, mdResultOpenSsl, CIPHER_BLOCKSIZE); // TODO avoid memory copy
}

void EncryptReader::decrypt_chunk(void *crypted_buffer, void* outputBuffer, uint32_t blkid)
{
	uint8_t iv[CIPHER_BLOCKSIZE];
	compute_iv(blkid, iv);
	AES_cbc_encrypt((uint8_t*)crypted_buffer, (uint8_t*)outputBuffer, m_crypt_blocksize, &m_aes_decrypt_key, iv, AES_DECRYPT);
}

int32_t EncryptReader::read(void* outputBuffer, int32_t size2, uint64_t off)
{
	uint8_t buffer[m_crypt_blocksize];
	uint8_t outputBufferTmp[m_crypt_blocksize];
	uint64_t mask = m_crypt_blocksize - 1;
	uint32_t blkid;
	int32_t rd_len;
	uint8_t *bdata = reinterpret_cast<uint8_t *>(outputBuffer);
	int32_t bytesLeft = size2;

	if (off & mask)
	{
		blkid = static_cast<uint32_t>(off / m_crypt_blocksize);

		if ( m_reader->read(buffer, m_crypt_blocksize, m_crypt_offset + (off & ~mask)) != m_crypt_blocksize )
			return int32_t(bdata - (uint8_t*)outputBuffer); // cast ok because it's not > size

		decrypt_chunk(buffer, outputBufferTmp, blkid);

		rd_len = m_crypt_blocksize - (off & mask);
		if (rd_len > bytesLeft)
			rd_len = bytesLeft;

		memcpy(bdata, outputBufferTmp + (off & mask), rd_len);

		bdata += rd_len;
		off += rd_len;
		bytesLeft -= rd_len;
	}

	while (bytesLeft > m_crypt_blocksize)
	{
		blkid = static_cast<uint32_t>(off / m_crypt_blocksize);

		if ( m_reader->read(buffer, m_crypt_blocksize, m_crypt_offset + (off & ~mask)) != m_crypt_blocksize ) // TODO can we remove & ~mask ?
			return int32_t(bdata - (uint8_t*)outputBuffer); // cast ok because it's not > size

		decrypt_chunk(buffer, bdata, blkid);

		rd_len = m_crypt_blocksize;

		bdata += rd_len;
		off += rd_len;
		bytesLeft -= rd_len;
	}

	blkid = static_cast<uint32_t>(off / m_crypt_blocksize);

	if ( m_reader->read(buffer, m_crypt_blocksize, m_crypt_offset + (off & ~mask)) != m_crypt_blocksize ) // TODO can we remove & ~mask ?
		return int32_t(bdata - (uint8_t*)outputBuffer); // cast ok because it's not > size

	decrypt_chunk(buffer, outputBufferTmp, blkid); // blkid for image, block_number

	memcpy(bdata, outputBufferTmp, bytesLeft);

	return size2;
}

uint64_t EncryptReader::length()
{
	return m_reader->length() - m_crypt_offset;
}
