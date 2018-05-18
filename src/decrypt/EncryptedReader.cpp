// the decryption code from
// https://github.com/sgan81/apfs-fuse/tree/master/ApfsLib

#include "EncryptedReader.h"
#include "TripleDes.h"
#include "Crypto.h"

#include "../be.h"

#include <vector>

#pragma pack(1)

struct DmgCryptHeaderV2
{
	char signature[8];
	uint32_t maybe_version;
	uint32_t unk_0C;
	uint32_t unk_10;
	uint32_t unk_14;
	uint32_t key_bits;
	uint32_t unk_1C;
	uint32_t unk_20;
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
	uint32_t blob_unk_5C;
	uint32_t blob_enc_mode;
	uint32_t encr_key_blob_size;
	uint8_t encr_key_blob[0x200];
};

#pragma pack()

EncryptedReader::EncryptedReader(std::shared_ptr<Reader> reader)
	: m_reader(reader), m_crypt_offset(0), m_crypt_size(0), m_crypt_blocksize(0)
{
	memset(m_hmac_key, 0, sizeof(m_hmac_key));
}

bool EncryptedReader::isEncryptedDMG(std::shared_ptr<Reader> reader) // static
{
	char encryptSignature[8];
	reader->read(&encryptSignature, sizeof(encryptSignature), 0);

	if (!memcmp(encryptSignature, "cdsaencr", 8))
		return false; // Encrypted DMG V1 detected. This is not supported.
	else if (!memcmp(encryptSignature, "encrcdsa", 8))
		return true;

	return false;
}

bool EncryptedReader::setupEncryptionV2(const std::string &password)
{
	bool key_ok = false;

	std::vector<uint8_t> data;
	std::vector<uint8_t> kdata;

	data.resize(0x1000);

	m_reader->read(reinterpret_cast<char *>(data.data()), data.size(), 0);

	DmgCryptHeaderV2 *hdr = reinterpret_cast<DmgCryptHeaderV2 *>(data.data());

	hdr->maybe_version = be(hdr->maybe_version);
	hdr->unk_0C = be(hdr->unk_0C);
	hdr->unk_10 = be(hdr->unk_10);
	hdr->unk_14 = be(hdr->unk_14);
	hdr->key_bits = be(hdr->key_bits);
	hdr->unk_1C = be(hdr->unk_1C);
	hdr->unk_20 = be(hdr->unk_20);
	hdr->block_size = be(hdr->block_size);
	hdr->encrypted_data_length = be(hdr->encrypted_data_length);
	hdr->encrypted_data_offset = be(hdr->encrypted_data_offset);
	hdr->no_of_keys = be(hdr->no_of_keys);

	m_crypt_offset = hdr->encrypted_data_offset;
	m_crypt_size = hdr->encrypted_data_length;
	m_crypt_blocksize = hdr->block_size;

	uint32_t no_of_keys = hdr->no_of_keys;

	for (uint32_t key_id = 0; key_id < no_of_keys; key_id++)
	{
		DmgKeyPointer *keyptr = reinterpret_cast<DmgKeyPointer *>(data.data() + sizeof(DmgCryptHeaderV2) + key_id * sizeof(DmgKeyPointer));

		keyptr->key_type = be(keyptr->key_type);
		keyptr->key_offset = be(keyptr->key_offset);
		keyptr->key_length = be(keyptr->key_length);

		kdata.resize(keyptr->key_length);

		m_reader->read(reinterpret_cast<char *>(kdata.data()), kdata.size(), keyptr->key_offset);

		DmgKeyData *keydata = reinterpret_cast<DmgKeyData *>(kdata.data());

		keydata->kdf_algorithm = be(keydata->kdf_algorithm);
		keydata->prng_algorithm = be(keydata->prng_algorithm);
		keydata->iteration_count = be(keydata->iteration_count);
		keydata->salt_len = be(keydata->salt_len);
		keydata->blob_enc_iv_size = be(keydata->blob_enc_iv_size);
		keydata->blob_enc_key_bits = be(keydata->blob_enc_key_bits);
		keydata->blob_enc_algorithm = be(keydata->blob_enc_algorithm);
		keydata->blob_unk_5C = be(keydata->blob_unk_5C);
		keydata->blob_enc_mode = be(keydata->blob_enc_mode);
		keydata->encr_key_blob_size = be(keydata->encr_key_blob_size);

		uint8_t derived_key[0x18];
		uint8_t blob[0x200];
		TripleDES des;

		PBKDF2_HMAC_SHA1(reinterpret_cast<const uint8_t *>(password.c_str()), password.size(), keydata->salt, keydata->salt_len, keydata->iteration_count, derived_key, sizeof(derived_key));

		des.SetKey(derived_key);
		des.SetIV(keydata->blob_enc_iv);

		uint32_t blob_len = keydata->encr_key_blob_size;

		des.DecryptCBC(blob, keydata->encr_key_blob, blob_len);

		if (blob[blob_len - 1] < 1 || blob[blob_len - 1] > 8)
			continue;

		blob_len -= blob[blob_len - 1];

		if (memcmp(blob + blob_len - 5, "CKIE", 4))
			continue;

        if (hdr->key_bits == 128)
		{
            m_aes.SetKey(blob, AES::AES_128);
			memcpy(m_hmac_key, blob + 0x10, 0x14);
			key_ok = true;
			break;
		}
		else if (hdr->key_bits == 256)
		{
            m_aes.SetKey(blob, AES::AES_256);
			memcpy(m_hmac_key, blob + 0x20, 0x14);
			key_ok = true;
			break;
		}
	}

	return key_ok;
}

int32_t EncryptedReader::read(void* buf, int32_t count, uint64_t offset)
{
	uint8_t buffer[0x1000];
	uint64_t mask = m_crypt_blocksize - 1;
	uint32_t blkid = 0;
	uint8_t iv[0x14];
    int32_t rd_len = 0;
	uint8_t *bdata = reinterpret_cast<uint8_t *>(buf);

	int32_t size = count;
	int32_t bytesRead = 0;

	if (offset & mask)
	{
		blkid = static_cast<uint32_t>(offset / m_crypt_blocksize);
		blkid = be(blkid);

		m_reader->read(reinterpret_cast<char *>(buffer), m_crypt_blocksize, m_crypt_offset + (offset & ~mask));

		HMAC_SHA1(m_hmac_key, 0x14, reinterpret_cast<const uint8_t *>(&blkid), sizeof(uint32_t), iv);

        m_aes.SetIV(iv);
        m_aes.DecryptCBC(buffer, buffer, m_crypt_blocksize);

		rd_len = m_crypt_blocksize - (offset & mask);
		
		if (rd_len > (count - bytesRead)) // buffer overflow check
			rd_len = count - bytesRead;

		memcpy(bdata, buffer + (offset & mask), rd_len);

		bdata += rd_len;
		offset += rd_len;
		size -= rd_len;

		bytesRead += rd_len;
	}

	while (size > 0)
	{
		blkid = static_cast<uint32_t>(offset / m_crypt_blocksize);
		blkid = be(blkid);

		m_reader->read(reinterpret_cast<char *>(buffer), m_crypt_blocksize, m_crypt_offset + (offset & ~mask));

		HMAC_SHA1(m_hmac_key, 0x14, reinterpret_cast<const uint8_t *>(&blkid), sizeof(uint32_t), iv);

        m_aes.SetIV(iv);
        m_aes.DecryptCBC(buffer, buffer, m_crypt_blocksize);

		rd_len = m_crypt_blocksize;
		
		if (rd_len > (count - bytesRead)) // buffer overflow check
            rd_len = count - bytesRead;

		memcpy(bdata, buffer, rd_len);

		bdata += rd_len;
		offset += rd_len;
		size -= rd_len;

		bytesRead += rd_len;
	}

	return bytesRead;
}

uint64_t EncryptedReader::length()
{
	return m_crypt_size;
}
