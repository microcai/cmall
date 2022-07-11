
#include "stdafx.hpp"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/process.hpp>

#include "services/neteasesdk.hpp"

#include "utils/timedmap.hpp"
#include "utils/logging.hpp"

#include "cmall/misc.hpp"
#include "cmall/js_util.hpp"
#include "utils/httpc.hpp"

using boost::asio::awaitable;
using boost::asio::use_awaitable;

namespace services
{
	struct neteasesdk_impl
	{
		neteasesdk_impl(boost::asio::io_context& io,
			const std::string& secret_id,
			const std::string& secret_key,
			const std::string& business_id
		)
			: io(io)
			, secret_id(secret_id)
			, secret_key(secret_key)
			, business_id(business_id)
		{}

		// verify the user input verify_code against verify_session
		awaitable<std::string> get_user_phone(std::string token, std::string accessToken)
		{
			static const std::string api_uri = "https://ye.dun.163yun.com/v1/oneclick/check";

			std::map<std::string, std::string> params;
			params.insert(std::make_pair("secretId", secret_id));
			params.insert(std::make_pair("businessId", business_id));
			params.insert(std::make_pair("version", "v1"));
			params.insert(std::make_pair("nonce", gen_nonce()));
			params.insert(std::make_pair("token", token));
			params.insert(std::make_pair("accessToken", accessToken));

			using namespace std::chrono;
			uint64_t ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
			params.insert(std::make_pair("timestamp", std::to_string(ms)));

			auto sign = gen_sig(params);
			params.insert(std::make_pair("signature", sign));

			std::string body;
			for (auto& [k, v] : params)
			{
				if (!body.empty())
					body += "&";
				body +=  k + "=" + v;
			}

			httpc::request_options_t option;
			option.url = api_uri;
			option.verb = httpc::http::verb::post;
			option.headers.insert(std::make_pair("Content-Type", "application/x-www-form-urlencoded"));
			option.body = body;

			auto reply = co_await request(option);
			auto resp = reply.body;

			LOG_DBG << "netease resp:" << reply.body;

			// { code, msg, data{ phone, resultCode } }
			do {
				boost::system::error_code ec;
				auto jv = boost::json::parse(resp, ec, {}, { 64, false, false, true });
				if (ec)
					break;

				boost::json::value code_value = jsutil::json_accessor(jv).get("code", boost::json::value{nullptr});
				if (!code_value.is_int64())
					break;

				auto code = code_value.as_int64();
				if (code != 200)
					break;

				boost::json::value data_value = jsutil::json_accessor(jv).get("data", boost::json::value{nullptr});
				if (!data_value.is_object())
					break;

				std::string phone = jsutil::json_accessor(data_value).get_string("phone");
				co_return phone;

			} while (false);

			co_return "";
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

		std::string gen_sig(const std::map<std::string, std::string>& params)
		{
			std::string to_sign;
			for (auto& [k, v] : params)
			{
				to_sign += k + v;
			}
			to_sign += secret_key;
			return md5sum(to_sign);
		}

		boost::asio::io_context& io;
		std::string secret_id;
		std::string secret_key;
		std::string business_id;
	};

	// verify the user input verify_code against verify_session
	awaitable<std::string> neteasesdk::get_user_phone(std::string token, std::string key)
	{
		return impl().get_user_phone(token, key);
	}

	neteasesdk::neteasesdk(boost::asio::io_context& io,
		const std::string& secret_id,
		const std::string& secret_key,
		const std::string& business_id)
	{
		static_assert(sizeof(obj_stor) >= sizeof(neteasesdk_impl));
		std::construct_at(reinterpret_cast<neteasesdk_impl*>(obj_stor.data()), io, secret_id, secret_key, business_id);
	}

	neteasesdk::~neteasesdk()
	{
		std::destroy_at(reinterpret_cast<neteasesdk_impl*>(obj_stor.data()));
	}

	const neteasesdk_impl& neteasesdk::impl() const
	{
		return *reinterpret_cast<const neteasesdk_impl*>(obj_stor.data());
	}

	neteasesdk_impl& neteasesdk::impl()
	{
		return *reinterpret_cast<neteasesdk_impl*>(obj_stor.data());
	}

}
