
#include "stdafx.hpp"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/process.hpp>
#include <boost/json.hpp>

#include "services/wxpay_service.hpp"

#include "utils/timedmap.hpp"
#include "utils/logging.hpp"

#include "cmall/misc.hpp"
#include "cmall/js_util.hpp"
#include "utils/httpc.hpp"

using boost::asio::awaitable;
using boost::asio::use_awaitable;

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

namespace services
{
	struct wxpay_service_impl
	{
		wxpay_service_impl(const std::string& rsa_key, const std::string& sp_appid, const std::string& sp_mchid, const std::string& notify_url)
			: rsa_key(rsa_key)
			, sp_mchid(sp_mchid)
			, sp_appid(sp_appid)
			, notify_url(notify_url)
		{}

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
			option.body = boost::json::serialize(params);

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

		std::string rsa_key;
		std::string sp_mchid;
		std::string sp_appid;
		std::string notify_url;
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

	wxpay_service::wxpay_service(const std::string& rsa_key, const std::string& sp_appid, const std::string& sp_mchid, const std::string& notify_url)
	{
		static_assert(sizeof(obj_stor) >= sizeof(wxpay_service_impl));
		std::construct_at(reinterpret_cast<wxpay_service_impl*>(obj_stor.data()), rsa_key, sp_appid, sp_mchid, notify_url);
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
