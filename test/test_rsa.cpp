
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>

#include <iostream>

#include <boost/beast.hpp>

inline std::string base64_encode(const std::string& in)
{
	auto b64_size = boost::beast::detail::base64::encoded_size(in.size());
	std::string ret;
	ret.resize(b64_size);

	boost::beast::detail::base64::encode(ret.data(), in.data(), in.size());

	return ret;
}

static inline std::string rsa_sign(const std::string& rsa_key_pem, const std::string& in)
{
	const unsigned char * p = reinterpret_cast<const unsigned char *>(rsa_key_pem.data());

    EVP_PKEY * pkey = nullptr;

	auto bio = std::unique_ptr<BIO, decltype(&BIO_free)> (BIO_new_mem_buf(rsa_key_pem.data(), rsa_key_pem.length()), BIO_free);

    pkey = PEM_read_bio_PrivateKey(bio.get(), &pkey, nullptr, nullptr);

	auto evp_private_key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> (pkey, EVP_PKEY_free);
	auto context = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> (EVP_MD_CTX_new(), EVP_MD_CTX_free);

	EVP_DigestSignInit(context.get(), nullptr,  EVP_sha256(), nullptr, evp_private_key.get());

	size_t siglen = 0;

	EVP_DigestSign(context.get(), nullptr, &siglen, reinterpret_cast<const unsigned char*>(in.data()), in.length());

	std::string signed_data;
	signed_data.resize(siglen);
	EVP_DigestSign(context.get(), reinterpret_cast<unsigned char*>(signed_data.data()), &siglen, reinterpret_cast<const unsigned char*>(in.data()), in.length());

	return base64_encode(signed_data);
}

const std::string key = R"_rkey(-----BEGIN RSA PRIVATE KEY-----
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
-----END RSA PRIVATE KEY-----
)_rkey";


const std::string test_cert = R"xxx(
-----BEGIN CERTIFICATE-----
MIIEKzCCAxOgAwIBAgIUTelyqSnLP00JOb5v3HqWDEgo5zcwDQYJKoZIhvcNAQEL
BQAwXjELMAkGA1UEBhMCQ04xEzARBgNVBAoTClRlbnBheS5jb20xHTAbBgNVBAsT
FFRlbnBheS5jb20gQ0EgQ2VudGVyMRswGQYDVQQDExJUZW5wYXkuY29tIFJvb3Qg
Q0EwHhcNMjMwMzI5MDczNDQ3WhcNMjgwMzI3MDczNDQ3WjCBhDETMBEGA1UEAwwK
MTUwMjc2NTg2MTEbMBkGA1UECgwS5b6u5L+h5ZWG5oi357O757ufMTAwLgYDVQQL
DCfmna3lt57ku6PnoIHpuL3mmbrog73np5HmioDmnInpmZDlhazlj7gxCzAJBgNV
BAYMAkNOMREwDwYDVQQHDAhTaGVuWmhlbjCCASIwDQYJKoZIhvcNAQEBBQADggEP
ADCCAQoCggEBAMc+g0yurzE77+AOQ7hsRzpiWN9bpW/f5JIgfRAw79doxF5a8oVm
ncAMfbvSQSWu7/LvgykUaFsZGuyynxdRzcwcGGjOhffadDjGOugWD01RD3X7Q6oN
bb2ocIiPtvIp2Ehn8VNxg70vLfGn9oj8pdA3P67vFcigSnP25xpKCiVQqOeVALGR
fA5Y7cC2+m56FtnjX5zyhQSkEB7ipSFYN3PTVFbWnN2fondTNcMW5rjm27vCLvKP
tPQAWAdv9r1o361fOVfLjnm5l6g9zkEWp+hrHP06ElJQfJHkfpffbj15lH/x6vkB
2YGyeLdKWteiMPiMSlgijG2vo+P49jr7ZXUCAwEAAaOBuTCBtjAJBgNVHRMEAjAA
MAsGA1UdDwQEAwID+DCBmwYDVR0fBIGTMIGQMIGNoIGKoIGHhoGEaHR0cDovL2V2
Y2EuaXRydXMuY29tLmNuL3B1YmxpYy9pdHJ1c2NybD9DQT0xQkQ0MjIwRTUwREJD
MDRCMDZBRDM5NzU0OTg0NkMwMUMzRThFQkQyJnNnPUhBQ0M0NzFCNjU0MjJFMTJC
MjdBOUQzM0E4N0FEMUNERjU5MjZFMTQwMzcxMA0GCSqGSIb3DQEBCwUAA4IBAQB/
zYz88CjCBqG+swXkzCvWTpvD4FMpE8IrdC8Fj7Nsw6YrD+zxZnbepkcXic3eswaa
w2O1knQbax0NyEZAvb+Oxv04t4L9cTIKVwiGRSq4geRLMrn3AbCrQ4wI1Xlj/LCX
fwonUZWjzrWdYtMdh5Fxr994SP/MZRbr5dxUv1t/eTEMcUyaAIE+PjNfoYQ5yZRH
rZw3lPVbsXY8oE5E8vYaWB+SgCYi5ar9uBF/o5spk+cZoasdxZEU8Uhg39thMMU5
EFWCZd09RNLs3EUv4MK7CPM72yujGFo2/F3MkR0Js+YvB4nD40fA4OWLxihb0daC
tgh4C1zYeDvr0DkUAN9o
-----END CERTIFICATE-----
)xxx";

std::string get_cert_serial(std::string rsa_cert)
{
	auto bio = std::unique_ptr<BIO, decltype(&BIO_free)> (BIO_new_mem_buf(rsa_cert.data(), rsa_cert.length()), BIO_free);
	X509* x = nullptr;
	auto x509_cert = std::unique_ptr<X509, decltype(&X509_free)> ( PEM_read_bio_X509(bio.get(), &x, nullptr, nullptr) , X509_free);

	auto asn1_serial_no = X509_get_serialNumber(x509_cert.get());
	auto serial_no_bn = std::unique_ptr<BIGNUM, decltype(&BN_free)> ( ASN1_INTEGER_to_BN(asn1_serial_no, nullptr) , BN_free);
	auto serial_no_char_xing = BN_bn2hex(serial_no_bn.get());
	std::string serial_no = serial_no_char_xing;
	OPENSSL_free(serial_no_char_xing);
	return serial_no;
}

static void handleErrors(){
	throw "ssl error";
}

static std::string aes_gcm_decrypt(std::string_view ciphertext_with_aead, std::string_view key, std::string_view iv, std::string_view aad)
{
    /* Create and initialise the context */
	auto ctx = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> (
		EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free
	);

	const auto AUTH_TAG_LENGTH_BYTE = 16;

	std::string_view tag = ciphertext_with_aead.substr(ciphertext_with_aead.length() - AUTH_TAG_LENGTH_BYTE, AUTH_TAG_LENGTH_BYTE);
	std::string_view ciphertext = ciphertext_with_aead.substr(ciphertext_with_aead.length() - AUTH_TAG_LENGTH_BYTE);

    int len;
	std::string plaintext;
	plaintext.resize(ciphertext.length() + key.length() + iv.length() + tag.length() + aad.length());
    int plaintext_len;
    int ret;

    /* Initialise the decryption operation. */
    if(!EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), NULL, NULL, NULL))
        handleErrors();

    /* Set IV length. Not necessary if this is 12 bytes (96 bits) */
    if(!EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, iv.length(), NULL))
        handleErrors();

    /* Initialise key and IV */
    if(!EVP_DecryptInit_ex(ctx.get(), NULL, NULL, reinterpret_cast<const unsigned char*>(key.data()), reinterpret_cast<const unsigned char*>(iv.data())))
        handleErrors();

    /*
     * Provide any AAD data. This can be called zero or more times as
     * required
     */
    if(!EVP_DecryptUpdate(ctx.get(), NULL, &len, reinterpret_cast<const unsigned char*>(aad.data()), aad.length()))
        handleErrors();

    /*
     * Provide the message to be decrypted, and obtain the plaintext output.
     * EVP_DecryptUpdate can be called multiple times if necessary
     */
    if(!EVP_DecryptUpdate(ctx.get(), reinterpret_cast<unsigned char*>(&plaintext[0]), &len, reinterpret_cast<const unsigned char*>(ciphertext.data()), ciphertext.length()))
        handleErrors();
    plaintext_len = len;

    /* Set expected tag value. Works in OpenSSL 1.0.1d and later */
    if(!EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, 16, const_cast<char*>(tag.data())))
        handleErrors();

    /*
     * Finalise the decryption. A positive return value indicates success,
     * anything else is a failure - the plaintext is not trustworthy.
     */
    ret = EVP_DecryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char*>(&plaintext[len]), &len);

    if(ret > 0) {
        /* Success */
        plaintext_len += len;
		plaintext.resize(plaintext_len);
		return plaintext;
    } else {
        /* Verify failed */
        return "";
    }
}

int main()
{
    const std::string data = "hello, world";

    std::cerr << rsa_sign(key, data) << std::endl;

    std::cerr << "test_cert serial no : "  << get_cert_serial(test_cert) << std::endl;

}