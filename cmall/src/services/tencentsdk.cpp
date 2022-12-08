
#include "stdafx.hpp"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/process.hpp>

#include "services/tencentsdk.hpp"

#include "utils/timedmap.hpp"
#include "utils/logging.hpp"

#include "cmall/misc.hpp"
#include "cmall/js_util.hpp"
#include "utils/httpc.hpp"

using boost::asio::awaitable;
using boost::asio::use_awaitable;

namespace services
{
	struct tencentsdk_impl
	{
		tencentsdk_impl(const std::string& secret_id, const std::string& secret_key)
			: secret_id(secret_id)
			, secret_key(secret_key)
		{}

		awaitable<boost::json::value> do_request(std::string service, std::string action, boost::json::object params)
		{
			boost::json::value ret;
			std::string api_url = "https://" + service + ".tencentcloudapi.com/";
			std::string region = "ap-shanghai";
			std::string api_version = "2020-03-03";
			std::string client_version = "SDK_CPP_3.0.634";
			std::string sign_method = "TC3-HMAC-SHA256";
			std::string body = jsutil::json_to_string(params, false);

			auto tp = boost::posix_time::second_clock::universal_time();
			std::time_t tm = boost::posix_time::to_time_t(tp);
			auto ts = std::to_string(tm);
			auto utc_date = boost::gregorian::to_iso_extended_string(tp.date());
			
			httpc::request_options_t option;
			option.url = api_url;
			option.verb = httpc::http::verb::post;
			option.headers.insert(std::make_pair("X-TC-Action", action));
			option.headers.insert(std::make_pair("X-TC-Version", api_version));
			option.headers.insert(std::make_pair("X-TC-Region", region));
			option.headers.insert(std::make_pair("X-TC-Timestamp", ts));
			option.headers.insert(std::make_pair("X-TC-RequestClient", client_version));
			option.headers.insert(std::make_pair("Connection", "Close"));
			option.headers.insert(std::make_pair("Content-Type", "application/json"));
			option.body = body;

			auto body_hash = sha256sum(body);
			std::string sign_headers = "content-type;host";
			std::string headers_to_sign = "content-type:application/json\nhost:" + service + ".tencentcloudapi.com\n";
			std::string request_to_sign = "POST\n/\n\n" + headers_to_sign + "\n" + sign_headers + "\n" + body_hash;
			auto request_hash = sha256sum(request_to_sign);

			std::string credential_scope = utc_date + "/" + service + "/tc3_request";
			std::string to_sign = sign_method + "\n" + ts + "\n" + credential_scope + "\n" + request_hash;

			auto kdate = hmac_sha256("TC3" + secret_key, utc_date, false);
			auto kservice = hmac_sha256(kdate, service, false);
			auto ksign = hmac_sha256(kservice, "tc3_request", false);
			auto sign = hmac_sha256(ksign, to_sign);
			std::string auth = std::format("TC3-HMAC-SHA256 Credential={}/{}, SignedHeaders={}, Signature=",secret_id, credential_scope, sign_headers, sign);
			option.headers.insert(std::make_pair("Authorization", auth));

			auto reply = co_await request(option);
			auto resp = reply.body;
			LOG_DBG << "tencentcloud resp:" << reply.body;

			boost::system::error_code ec;
			ret = boost::json::parse(resp, ec, {}, { 64, false, false, true });
			co_return ret;
		}

		std::string secret_id;
		std::string secret_key;
	};

	tencentsdk::tencentsdk(const std::string& secret_id, const std::string& secret_key)
	{
		static_assert(sizeof(obj_stor) >= sizeof(tencentsdk_impl));
		std::construct_at(reinterpret_cast<tencentsdk_impl*>(obj_stor.data()), secret_id, secret_key);
	}

	tencentsdk::~tencentsdk()
	{
		std::destroy_at(reinterpret_cast<tencentsdk_impl*>(obj_stor.data()));
	}

	awaitable<bool> tencentsdk::group_create(std::string gid, std::string gname)
	{
		boost::json::object params;
		params["GroupId"] = gid;
		params["GroupName"] = gname;
		params["FaceModelVersion"] = "3.0";

		auto res = co_await impl().do_request("iai", "CreateGroup", params);
		co_return true;
	}
	awaitable<bool> tencentsdk::group_add_person(std::string gid, std::string pid, std::string pname, std::string input)
	{
		boost::json::object params;
		params["GroupId"] = gid;
		params["PersonId"] = pid;
		params["PersonName"] = pname;
		if (input.starts_with("http")) 
			params["Url"] = input;
		else
			params["Image"] = input;

		auto res = co_await impl().do_request("iai", "CreatePerson", params);
		co_return true;
	}
	// 查询人员归属. GetPersonGroupInfo
	awaitable<std::vector<std::string>> tencentsdk::get_person_group(std::string pid)
	{
		boost::json::object params;
		params["PersonId"] = pid;

		auto res = co_await impl().do_request("iai", "GetPersonGroupInfo", params);

		std::vector<std::string> ret;
		try {
			auto person_groups = res.at("Response").at("PersonGroupInfos").as_array();
			for (auto& it : person_groups)
			{
				auto gid = it.at("GroupId").as_string();
				ret.push_back(std::string(gid.begin(), gid.end()));
			}
		}
		catch (...) {

		}
		co_return ret;
	}
	awaitable<bool> tencentsdk::person_add_face(std::string pid, std::string input)
	{
		boost::json::object params;
		params["PersonId"] = pid;
		boost::json::array in = { input };
		if (input.starts_with("http")) 
			params["Urls"] = in;
		else
			params["Images"] = in;

		auto res = co_await impl().do_request("iai", "CreateFace", params);
		co_return true;
	}
	awaitable<std::string> tencentsdk::person_search_by_face(std::string gid, std::string input)
	{
		boost::json::object params;
		boost::json::array gids = { gid };
		params["GroupIds"] = gids;
		params["FaceMatchThreshold"] = 80;
		params["NeedPersonInfo"] = 1;
		if (input.starts_with("http")) 
			params["Url"] = input;
		else
			params["Image"] = input;

		boost::json::value res = co_await impl().do_request("iai", "SearchPersionsReturnsByGroup", params);
		/*
			{
			  Response: {
				RequestId,
				FaceModelVersoin,
				PersonNum,
				ResultsReturnsByGroup: [
				  {
				    RetCode: 0,
					FaceRect: {}
					GroupCandidates: [
					  {
					    GroupId,
						Candidates: [
						  {
						    PersonId,
							Score,
							PersonName,
							PersonGroupInfos: []
						  }
						]
					  }
					]
				  }
				]
			  }
			}
		 */ 
		do {
			try {
				auto results = res.at("Response").at("ResultsReturnsByGroup").as_array();
				auto result = results.at(0);
				if (result.at("RetCode").as_int64() != 0)
					break;
				auto group_candidates = result.at("GroupCandidates").as_array();
				auto candidates = group_candidates.at(0).at("Candidates").as_array();
				auto match = candidates.at(0);
				auto id = match.at("PersonId").as_string();
				co_return std::string(id.begin(), id.end());
			}
			catch (...) {
				break;
			}
		} while (false);

		co_return std::string{};
	}

	const tencentsdk_impl& tencentsdk::impl() const
	{
		return *reinterpret_cast<const tencentsdk_impl*>(obj_stor.data());
	}

	tencentsdk_impl& tencentsdk::impl()
	{
		return *reinterpret_cast<tencentsdk_impl*>(obj_stor.data());
	}

}
