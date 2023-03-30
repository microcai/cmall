
#include "stdafx.hpp"

#include <openssl/core_names.h>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/process.hpp>
#include <boost/json.hpp>

#include "services/wxpay_service.hpp"

#include "cmall/internal.hpp"
#include "utils/timedmap.hpp"
#include "utils/logging.hpp"

#include "cmall/misc.hpp"
#include "cmall/js_util.hpp"
#include "utils/httpc.hpp"
#include "magic_enum.hpp"
#include "cmall/error_code.hpp"
#include "cmall/conversion.hpp"

using boost::asio::awaitable;
using boost::asio::use_awaitable;
using pay_status = services::weixin::pay_status;
using notify_message = services::weixin::notify_message;

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

static inline bool rsa_sign_verify(const std::string& rsa_cert_pem, const std::string& in, const std::string& signature)
{
	const unsigned char * p = reinterpret_cast<const unsigned char *>(rsa_cert_pem.data());

    EVP_PKEY * pkey = nullptr;

	auto bio = std::unique_ptr<BIO, decltype(&BIO_free)> (BIO_new_mem_buf(rsa_cert_pem.data(), rsa_cert_pem.length()), BIO_free);
	auto x509_cert = std::unique_ptr<X509, decltype(&X509_free)> ( PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr) , X509_free);

    pkey = X509_get0_pubkey(x509_cert.get());

	auto context = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> (EVP_MD_CTX_new(), EVP_MD_CTX_free);

	EVP_DigestVerifyInit(context.get(), nullptr,  EVP_sha256(), nullptr, pkey);

	return EVP_DigestVerify(context.get(), reinterpret_cast<const unsigned char*>(signature.data()), signature.length(), reinterpret_cast<const unsigned char*>(in.data()), in.length());
}

static void handleErrors(){
	throw boost::system::system_error(cmall::error::make_error_code(cmall::error::internal_server_error));
}

static std::string aes_gcm_decrypt(std::string_view ciphertext_with_aead, std::string_view key, std::string_view iv, std::string_view aad)
{
    /* Create and initialise the context */
	auto ctx = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> (
		EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free
	);

	const auto AUTH_TAG_LENGTH_BYTE = 16;

	std::string_view tag = ciphertext_with_aead.substr(ciphertext_with_aead.length() - AUTH_TAG_LENGTH_BYTE, AUTH_TAG_LENGTH_BYTE);
	std::string_view ciphertext = ciphertext_with_aead.substr(0, ciphertext_with_aead.length() - AUTH_TAG_LENGTH_BYTE);

    int len;
	std::string plaintext;
	plaintext.resize(ciphertext.length() + key.length() + iv.length() + tag.length() + aad.length());
    int plaintext_len;
    int ret;

	int iv_len = iv.length();

	OSSL_PARAM gcm_param[3] = {
		{ OSSL_CIPHER_PARAM_IVLEN, OSSL_PARAM_INTEGER, &iv_len ,sizeof(iv_len) , 0},
		{ OSSL_CIPHER_PARAM_AEAD_TAG, OSSL_PARAM_OCTET_STRING, const_cast<char*>(tag.data()) ,tag.length() , 0},
		{ nullptr, 0, nullptr, 0, 0 }
	};

    /* Initialise the decryption operation. and set key and IV and AEAD data */
    if(!EVP_DecryptInit_ex2(ctx.get(), EVP_aes_256_gcm(), reinterpret_cast<const unsigned char*>(key.data()), reinterpret_cast<const unsigned char*>(iv.data()), gcm_param))
    {
	    handleErrors();
	}

    /*
     * Provide any AAD data. This can be called zero or more times as
     * required
     */
    if(!EVP_DecryptUpdate(ctx.get(), nullptr, &len, reinterpret_cast<const unsigned char*>(aad.data()), aad.length()))
        handleErrors();

    // EVP_CIPHER_CTX_set_padding(ctx.get(), 1);

    /*
     * Provide the message to be decrypted, and obtain the plaintext output.
     * EVP_DecryptUpdate can be called multiple times if necessary
     */
    if(!EVP_DecryptUpdate(ctx.get(), reinterpret_cast<unsigned char*>(&plaintext[0]), &len, reinterpret_cast<const unsigned char*>(ciphertext.data()), ciphertext.length()))
        handleErrors();
    plaintext_len = len;

    /*
     * Finalise the decryption. A positive return value indicates success,
     * anything else is a failure - the plaintext is not trustworthy.
     */
	int f_len = plaintext.length() - plaintext_len;
    ret = EVP_DecryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char*>(&plaintext[len]), &f_len);

    if(ret > 0)
	{
        /* Success */
        plaintext_len += f_len;
		plaintext.resize(plaintext_len);
		return plaintext;
    }
	else
	{
		ERR_print_errors_fp(stderr);

        /* Verify failed */
        return "";
    }
}

struct wx_encrypt_data{
	std::string original_type;
	std::string algorithm;
	std::string ciphertext;
	std::string associated_data;
	std::string nonce;
};

struct notify_object{
	std::string event_type;
	boost::posix_time::ptime create_time;
	wx_encrypt_data resource;
};

struct encrypt_certificate{
	std::string serial_no;
	boost::posix_time::ptime expire_time;
	wx_encrypt_data certificate;
};

inline namespace conversion
{
	// deserialization
	wx_encrypt_data tag_invoke(const boost::json::value_to_tag<wx_encrypt_data>&, const boost::json::value& jv)
	{
		using namespace boost::json;

		if (!jv.is_object())
			return {};

		const auto& obj = jv.get_object();

		if (!obj.contains("ciphertext") || !obj.contains("nonce"))
			return {};

		wx_encrypt_data ret;

		if (obj.contains("original_type"))
			ret.original_type = value_to<std::string>(obj.at("original_type"));
		if (obj.contains("algorithm"))
			ret.algorithm = value_to<std::string>(obj.at("algorithm"));
		if (obj.contains("associated_data"))
			ret.associated_data = value_to<std::string>(obj.at("associated_data"));

		ret.ciphertext = value_to<std::string>(obj.at("ciphertext"));
		ret.nonce = value_to<std::string>(obj.at("nonce"));

		return ret;
	}

	std::optional<notify_object> tag_invoke(const boost::json::value_to_tag<std::optional<notify_object>>&, const boost::json::value& jv)
	{
		using namespace boost::json;

		if (!jv.is_object())
			return {};

		const auto& obj = jv.get_object();

		if (!obj.contains("resource") || !obj.at("resource").is_object())
			return {};

		notify_object ret;

		ret.event_type = value_to<std::string>(obj.at("event_type"));
		ret.create_time = value_to<boost::posix_time::ptime>(obj.at("create_time"));
		ret.resource = value_to<wx_encrypt_data>(obj.at("resource"));

		return ret;
	}

	std::optional<encrypt_certificate> tag_invoke(const boost::json::value_to_tag<std::optional<encrypt_certificate>>&, const boost::json::value& jv)
	{
		using namespace boost::json;

		if (!jv.is_object())
			return {};

		const auto& obj = jv.get_object();

		if (!obj.contains("encrypt_certificate") || !obj.at("encrypt_certificate").is_object())
			return {};

		encrypt_certificate ret;

		ret.serial_no = value_to<std::string>(obj.at("serial_no"));
		ret.certificate = value_to<wx_encrypt_data>(obj.at("encrypt_certificate"));
		ret.expire_time = value_to<boost::posix_time::ptime>(obj.at("expire_time"));

		return ret;
	}
}

namespace services::weixin {
	notify_message tag_invoke(const boost::json::value_to_tag<notify_message>&, const boost::json::value& jv)
	{
		using namespace boost::json;

		if (!jv.is_object())
			return {};

		const auto& obj = jv.get_object();

		if (!obj.contains("out_trade_no") || !obj.contains("transaction_id"))
			return {};

		notify_message ret;

		ret.out_trade_no = value_to<std::string>(obj.at("out_trade_no"));

		ret.trade_state = magic_enum::enum_cast<pay_status>(value_to<std::string>(obj.at("trade_state"))).value_or(pay_status::QUERY_FAILED);
		ret.success_time = value_to<boost::posix_time::ptime>(obj.at("success_time"));

		auto amount = obj.at("amount").as_object();

		ret.order_amount = cpp_numeric( value_to<long>(amount.at("total")) ) / 100;
		ret.payed_amount = cpp_numeric( value_to<long>(amount.at("payer_total")) ) / 100;

		return ret;
	}
}

namespace services
{
	struct wxpay_service_impl
	{
		wxpay_service_impl(const std::string& rsa_key, const std::string& rsa_cert, const std::string& apiv3_key, const std::string& sp_appid, const std::string& sp_mchid, const std::string& notify_url)
			: rsa_key(rsa_key)
			, rsa_cert(rsa_cert)
			, apiv3_key(apiv3_key)
			, sp_mchid(sp_mchid)
			, sp_appid(sp_appid)
			, notify_url(notify_url)
		{}

		void reinit(const std::string& rsa_key, const std::string& rsa_cert, const std::string& apiv3_key, const std::string& sp_appid, const std::string& sp_mchid, const std::string& notify_url)
		{
			this->rsa_key = rsa_key;
			this->rsa_cert = rsa_cert;
			this->apiv3_key = apiv3_key;
			this->sp_appid = sp_appid;
			this->sp_mchid = sp_mchid;
			this->notify_url = notify_url;
		}

		// verify the user input verify_code against verify_session
		awaitable<std::string> get_prepay_id(std::string sub_mchid, std::string out_trade_no, cpp_numeric amount, std::string goods_description, std::string payer_openid)
		{
			static const std::string api_uri = "https://api.mch.weixin.qq.com/v3/pay/partner/transactions/jsapi";

			using namespace std::string_literals;

			std::int64_t ammount_in_cents = (amount * 100).convert_to<std::int64_t>();

			boost::json::object payer_obj = {
				{ "sp_openid"s, payer_openid },
			};

			boost::json::object amount_obj = {
				{ "currency"s, "CNY"s },
				{ "total"s, ammount_in_cents },
			};

			boost::json::object params = {
				{ "sp_mchid"s, sp_mchid },
				{ "sp_appid"s, sp_appid },
				{ "sub_mchid"s, sub_mchid },
				{ "out_trade_no"s, out_trade_no },
				{ "description"s, goods_description },
				{ "notify_url"s, notify_url },
				{ "payer"s, payer_obj },
				{ "amount"s, amount_obj },
			};

			httpc::request_options_t option;
			option.url = api_uri;
			option.verb = httpc::http::verb::post;
			option.headers.insert(std::make_pair("Content-Type", "application/json"));
			option.headers.insert(std::make_pair("Accept", "application/json"));
			option.headers.insert(std::make_pair("User-Agent", "c++mall"));
			option.body = boost::json::serialize(params);
			option.headers.insert(std::make_pair("Authorization", gen_REST_auth_header("POST", "/v3/pay/partner/transactions/jsapi", option.body.value())));

			auto reply = co_await request(option);
			auto resp = reply.body;

			LOG_DBG << "wxpay resp:" << reply.body;

			// { code, msg, data{ phone, resultCode } }
			do {
				boost::system::error_code ec;
				auto jv = boost::json::parse(resp, ec, {}, { 64, false, false, true });
				if (ec)
					break;

				boost::json::value prepay_id_value = jsutil::json_accessor(jv).get("prepay_id", boost::json::value{nullptr});
				if (!prepay_id_value.is_string())
					break;

				auto prepay_id = prepay_id_value.as_string();

				co_return prepay_id;

			} while (false);

			co_return "";
		}

		awaitable<boost::json::object> get_pay_object(std::string prepay_id)
		{
			using namespace std::string_literals;

			std::string timeStamp = fmt::format("{}", std::time(nullptr));
			std::string nonceStr = gen_nonce();
			std::string package = fmt::format("prepay_id={}", prepay_id);

			gen_sig(timeStamp, nonceStr, package);

			boost::json::object params = {
				{ "timeStamp"s, timeStamp },
				{ "nonceStr"s, nonceStr },
				{ "package"s, package },
				{ "signType"s, "RSA"s },
				{ "paySign"s, gen_sig(timeStamp, nonceStr, package) },
			};

			co_return params;
		}

		awaitable<weixin::pay_status> is_payed(std::string orderid, std::string sub_mchid_of_weixin)
		{
			httpc::request_options_t option;

			// DOC: https://pay.weixin.qq.com/wiki/doc/apiv3_partner/apis/chapter4_5_2.shtml
			option.url = fmt::format("https://api.mch.weixin.qq.com/v3/pay/partner/transactions/out-trade-no/{}?sp_mchid={}&sub_mchid={}", orderid, sp_mchid, sub_mchid_of_weixin);
			option.verb = httpc::http::verb::get;
			option.headers.insert(std::make_pair("Connection", "Close"));

			auto reply = co_await httpc::request(option);
			boost::system::error_code ec;
			auto wx_response = boost::json::parse(reply.body, ec, {}, { 64, false, false, true });

            auto trade_state = jsutil::json_accessor(wx_response).get_string("trade_state");

			auto trade_state_enum = magic_enum::enum_cast<pay_status>(trade_state);

			if (trade_state_enum.has_value())
				co_return trade_state_enum.value();
			co_return pay_status::QUERY_FAILED;
		}

		// 这个接口用于解码 微信 发送的回调信息.
		// 并且需要使用 HTTP 请求头部里的 Wechatpay-Signature 字段来验证消息可靠性
		// 验证 Wechatpay_Signature 所需的证书, 则需要定期从 腾讯刷.
		// 刷 证书 的方式应该也由本 service 自动完成.
		// 最好是程序硬编码一个 证书, 以便 刷新例程工作完成前使用.
		awaitable<notify_message> decode_notify_message(std::string_view notify_body, std::string_view Wechatpay_Timestamp, std::string_view Wechatpay_Nonce, std::string_view Wechatpay_Signature)
		{
			if (!verify_sig(notify_body, Wechatpay_Timestamp, Wechatpay_Nonce, Wechatpay_Signature))
				throw boost::system::system_error(cmall::error::make_error_code(cmall::error::nofity_message_signature_invalid));

			// DOC: https://pay.weixin.qq.com/wiki/doc/apiv3_partner/apis/chapter4_5_5.shtml
			notify_message ret;

			auto notify_object_json = boost::json::parse(notify_body);
			auto notify_obj = boost::json::value_to<std::optional<notify_object>>(notify_object_json);

			auto decoded_notify_msg_str = decode_wx_encrypt_data(notify_obj->resource.ciphertext, notify_obj->resource.nonce, notify_obj->resource.associated_data);

			auto decoded_notify_msg_jv = boost::json::parse(decoded_notify_msg_str);

			co_return boost::json::value_to<notify_message>(decoded_notify_msg_jv);
		}

		std::string gen_REST_auth_header(std::string VERB, std::string_view url, std::string_view body)
		{
			std::string timeStamp = fmt::format("{}", std::time(nullptr));
			std::string nonceStr = gen_nonce();

			auto signature = gen_sig_v3(VERB, url, timeStamp, nonceStr, body);

			auto bio = std::unique_ptr<BIO, decltype(&BIO_free)> (BIO_new_mem_buf(rsa_cert.data(), rsa_cert.length()), BIO_free);
			auto x509_cert = std::unique_ptr<X509, decltype(&X509_free)> ( PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr) , X509_free);

			auto asn1_serial_no = X509_get_serialNumber(x509_cert.get());
			auto serial_no_bn = std::unique_ptr<BIGNUM, decltype(&BN_free)> ( ASN1_INTEGER_to_BN(asn1_serial_no, nullptr) , BN_free);
			auto serial_no_char_xing = BN_bn2hex(serial_no_bn.get());
			std::string serial_no = serial_no_char_xing;
			OPENSSL_free(serial_no_char_xing);

			std::string auth = std::format(R"auth(WECHATPAY2-SHA256-RSA2048 mchid="{}",nonce_str="{}",signature="{}",timestamp="{}",serial_no="{}")auth",
				sp_mchid, nonceStr,  signature, timeStamp, serial_no);

			return auth;
		}

		std::string gen_nonce(std::size_t len = 32)
		{
			static const std::string chartable = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
			thread_local std::mt19937 urng{ std::random_device{}() };
			std::string result;
			result.resize(len);
			std::sample(chartable.begin(), chartable.end(), result.begin(), len, urng);
			return result;
		}

		std::string gen_sig(std::string timeStamp, std::string nonceStr, std::string package)
		{
			std::string to_sign = fmt::format("{}\n{}\n{}\n{}\n", sp_appid, timeStamp, nonceStr, package);

			return rsa_sign(rsa_key,to_sign);
		}

		std::string gen_sig_v3(std::string_view verb, std::string_view path, std::string_view timeStamp, std::string_view nonceStr, std::string_view body)
		{
			std::string to_sign = fmt::format("{}\n{}\n{}\n{}\n{}\n", verb, path, timeStamp, nonceStr, body);

			return rsa_sign(rsa_key,to_sign);
		}

		bool verify_sig(std::string_view body, std::string_view Wechatpay_Timestamp, std::string_view Wechatpay_Nonce, std::string_view Wechatpay_Signature)
		{
			std::string to_verify = fmt::format("{}\n{}\n{}\n", Wechatpay_Timestamp, Wechatpay_Nonce, body);

			std::scoped_lock<std::mutex> l(key_protect);

			return rsa_sign_verify(Wechatpay_Signature_Key, to_verify, base64_decode(Wechatpay_Signature));
		}

		std::string decode_wx_encrypt_data(std::string_view ciphertext, std::string_view nonce, std::string_view associated_data)
		{
			auto decrypted = aes_gcm_decrypt(base64_decode(ciphertext), apiv3_key, nonce, associated_data);

			return decrypted;
		}

		// 每调用一次, 下载一次最新的证书.
		// TODO, 添加一个定时任务, 每12小时调用一次这个接口.
		awaitable<void> download_latest_wxpay_cert()
		{
			httpc::request_options_t option;
			option.verb = httpc::http::verb::get;
			option.url = "https://api.mch.weixin.qq.com/v3/certificates";

			option.headers.insert(std::make_pair("Accept", "application/json"));
			option.headers.insert(std::make_pair("User-Agent", "c++mall"));
			option.headers.insert(std::make_pair("Authorization", gen_REST_auth_header("GET", "/v3/certificates", "")));

			auto wx_response = co_await httpc::request(option);

			for (auto& h : wx_response)
				std::cout << h.name_string() << ":" << h.value() << std::endl;
			std::cerr << wx_response.body << std::endl;

			if (wx_response.code == 200)
			{
				bool verify_ok = this->verify_sig(wx_response.body, wx_response["Wechatpay-Timestamp"], wx_response["Wechatpay-Nonce"], wx_response["Wechatpay-Signature"]);

				if (!verify_ok)
				{
					std::cerr << "wx response body verify failed!\n";
					co_return;
				}

				std::cerr << "wx response body is verified!\n";

				auto response_body = boost::json::parse(wx_response.body, {}, { 64, false, false, true });
				auto encryped_certs = jsutil::json_accessor(response_body).get_array("data");

				std::vector<encrypt_certificate> certs;

				for (auto & jv : encryped_certs)
				{
					auto encryped_cert = boost::json::value_to<std::optional<encrypt_certificate>>(jv);

					if (encryped_cert.has_value())
						certs.push_back(encryped_cert.value());
				}

				std::sort(certs.begin(), certs.end(),
					[](auto & a, auto& b)
					{
						return a.expire_time > b.expire_time;
					});

				auto latest_cert_decoded = decode_wx_encrypt_data(certs[0].certificate.ciphertext, certs[0].certificate.nonce, certs[0].certificate.associated_data);

				if (!latest_cert_decoded.empty())
				{
					std::scoped_lock<std::mutex> l(key_protect);
					this->Wechatpay_Signature_Key = latest_cert_decoded;
					std::cerr << "tencent cert update as \n";
					std::cerr << latest_cert_decoded;
					std::cerr << "tencent cert updated\n";
				}
			}
			co_return ;
		}

		std::string rsa_key;
		std::string rsa_cert;

		std::string apiv3_key;

		std::string sp_mchid;
		std::string sp_appid;
		std::string notify_url;

		// 初始化一个硬编码的代码写作日的最新版.
		// 然后开启一个定时任务, 每隔 12小时从腾讯获取新的证书.
		// 获取的文档在 https://pay.weixin.qq.com/wiki/doc/apiv3/wechatpay/wechatpay4_1.shtml
		std::string Wechatpay_Signature_Key = R"pubkey(-----BEGIN CERTIFICATE-----
MIIEFDCCAvygAwIBAgIUUnfl6FXi41QzLxUx6CQeFEJcJZAwDQYJKoZIhvcNAQEL
BQAwXjELMAkGA1UEBhMCQ04xEzARBgNVBAoTClRlbnBheS5jb20xHTAbBgNVBAsT
FFRlbnBheS5jb20gQ0EgQ2VudGVyMRswGQYDVQQDExJUZW5wYXkuY29tIFJvb3Qg
Q0EwHhcNMjMwMzI5MDczNDQ2WhcNMjgwMzI3MDczNDQ2WjBuMRgwFgYDVQQDDA9U
ZW5wYXkuY29tIHNpZ24xEzARBgNVBAoMClRlbnBheS5jb20xHTAbBgNVBAsMFFRl
bnBheS5jb20gQ0EgQ2VudGVyMQswCQYDVQQGDAJDTjERMA8GA1UEBwwIU2hlblpo
ZW4wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCut23n2Z6pbZPDDKFM
njvzOkFTuxhjYdlzTs98RXE+4cJtZurs8BcWea2HmVeI92zdoi8AtU9z0Ho9Y0HP
V23YipGM7pKGmE8ZkKSt2uFrXFuAfYDIdRhsot5GhvzCblq50TnVD7VYvyV0+d4z
nA/7DduJCvpjcWarFAzWQR3Lrar+566L0gkVbPmXzUNwTmPBH9Xmm60oyP4G1kVO
Y2Dc+uwlriJbnrQY6n1SyC4FD1nuhii1Zo3vun1d6fPkHH+gaWnk61jXgrkY7m2l
NxA2TjMEGhy5LLJ5EBcN6nagYd1C5G37dbAO1bjmQ1TVn1ttNrtwAuOQxgWHtKQx
DlCXAgMBAAGjgbkwgbYwCQYDVR0TBAIwADALBgNVHQ8EBAMCA/gwgZsGA1UdHwSB
kzCBkDCBjaCBiqCBh4aBhGh0dHA6Ly9ldmNhLml0cnVzLmNvbS5jbi9wdWJsaWMv
aXRydXNjcmw/Q0E9MUJENDIyMEU1MERCQzA0QjA2QUQzOTc1NDk4NDZDMDFDM0U4
RUJEMiZzZz1IQUNDNDcxQjY1NDIyRTEyQjI3QTlEMzNBODdBRDFDREY1OTI2RTE0
MDM3MTANBgkqhkiG9w0BAQsFAAOCAQEAQ9LZ4vugkmAHEwrKv2b4jKfUWEo3BGbW
NaMlEXjwBvut2PbZBf3q2xscCzqE+LGM9h4BSuvWTW7m8vvdB+vyMiDHcJcrXH+8
xxvfYfDOc3uQfCgunz9WbZBaMHqy4daFXHqSHUivTMOcguR5TKCFdGprsH78HdfF
ZzI1LpHqDbshfj+Khwfw/0/mRSycMbLL2eaFKFfh1yylhKZhC2BL0ypZLLSgLzmO
C2qhuN3DwCgp9rUbfx+kh3GnOl6NDt5+HZmiw554i2YdcTUyNVEm07/vVUoNOHNP
V739kLuz6XvUpJo3EzM0OGT1TjikmyUcNkNQjZj/yEeQwRkp+lVoew==
-----END CERTIFICATE-----)pubkey";
		std::mutex key_protect;
	};

	// verify the user input verify_code against verify_session
	awaitable<std::string> wxpay_service::get_prepay_id(std::string sub_mchid, std::string out_trade_no, cpp_numeric amount, std::string goods_description, std::string payer_openid)
	{
		co_return co_await impl().get_prepay_id(sub_mchid, out_trade_no, amount, goods_description, payer_openid);
	}

	awaitable<boost::json::object> wxpay_service::get_pay_object(std::string prepay_id)
	{
		co_return co_await impl().get_pay_object(prepay_id);
	}

	wxpay_service::wxpay_service(const std::string& rsa_key, const std::string& rsa_cert, const std::string& apiv3_key, const std::string& sp_appid, const std::string& sp_mchid, const std::string& notify_url)
	{
		static_assert(sizeof(obj_stor) >= sizeof(wxpay_service_impl));
		std::construct_at(reinterpret_cast<wxpay_service_impl*>(obj_stor.data()), rsa_key, rsa_cert, apiv3_key, sp_appid, sp_mchid, notify_url);
	}

	void wxpay_service::reinit(const std::string& rsa_key, const std::string& rsa_cert, const std::string& apiv3_key, const std::string& sp_appid, const std::string& sp_mchid, const std::string& notify_url)
	{
		impl().reinit(rsa_key, rsa_cert, apiv3_key, sp_appid, sp_mchid, notify_url);
	}

	awaitable<notify_message> wxpay_service::decode_notify_message(std::string_view notify_body, std::string_view Wechatpay_Timestamp, std::string_view Wechatpay_Nonce, std::string_view Wechatpay_Signature)
	{
		co_return co_await impl().decode_notify_message(notify_body, Wechatpay_Timestamp, Wechatpay_Nonce, Wechatpay_Signature);
	}

	awaitable<void> wxpay_service::download_latest_wxpay_cert()
	{
		co_return co_await impl().download_latest_wxpay_cert();
	}

	wxpay_service::~wxpay_service()
	{
		std::destroy_at(reinterpret_cast<wxpay_service_impl*>(obj_stor.data()));
	}

	const wxpay_service_impl& wxpay_service::impl() const
	{
		return *reinterpret_cast<const wxpay_service_impl*>(obj_stor.data());
	}

	wxpay_service_impl& wxpay_service::impl()
	{
		return *reinterpret_cast<wxpay_service_impl*>(obj_stor.data());
	}

}
