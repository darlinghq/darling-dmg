#pragma once

#include <memory>
#include <string>
#include "../Reader.h"

#include "Aes.h"

class EncryptedReader : public Reader
{
public:

	EncryptedReader(std::shared_ptr<Reader> reader);

	int32_t read(void* buf, int32_t count, uint64_t offset) override;
	uint64_t length() override;

	bool setupEncryptionV2(const std::string &password);

	static bool isEncryptedDMG(std::shared_ptr<Reader> reader);

protected:

private:

        std::shared_ptr<Reader> m_reader;

	uint64_t m_crypt_offset;
	uint64_t m_crypt_size;
	uint32_t m_crypt_blocksize;
	uint8_t m_hmac_key[0x14];

        AES m_aes;
};
