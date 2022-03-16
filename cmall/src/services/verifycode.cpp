
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/process.hpp>

#include "services/verifycode.hpp"

#include "utils/timedmap.hpp"
#include "utils/logging.hpp"

namespace services
{
	static std::string uuid_to_string(boost::uuids::uuid const& u)
	{
		std::string result;
		result.reserve(36);

		std::size_t i = 0;
		boost::uuids::uuid::const_iterator it_data = u.begin();
		for (; it_data != u.end(); ++it_data, ++i)
		{
			const size_t hi = ((*it_data) >> 4) & 0x0F;
			result += boost::uuids::detail::to_char(hi);

			const size_t lo = (*it_data) & 0x0F;
			result += boost::uuids::detail::to_char(lo);
		}
		return result;
	}

	static std::string gen_uuid()
	{
		return uuid_to_string(boost::uuids::random_generator()());
	}

	struct verifycode_impl
	{
		verifycode_impl(boost::asio::io_context& io)
			: io(io)
			, verify_code_stor(io, std::chrono::minutes(5))
		{}

		// send verifycode, returns verify session.
		boost::asio::awaitable<verify_session> send_verify_code(std::string telephone, boost::system::error_code& ec)
		{
			boost::process::async_pipe ap(io);

			boost::process::child node_js_code_sender(io, boost::process::search_path("sendsms_verify"), "--phone", telephone, boost::process::std_out > ap);

			std::string sended_smscode;

			auto d_buffer = boost::asio::dynamic_buffer(sended_smscode);

			auto read_sms_output = co_await boost::asio::async_read_until(ap, d_buffer, '\n', boost::asio::use_awaitable);

			boost::algorithm::trim_right_if(sended_smscode, boost::is_any_of(" \n\r"));

			LOG_INFO << "sendsms_verify returned: " << sended_smscode;

			verify_session ret;

			ret.session_cookie = gen_uuid();

			verify_code_stor.put(ret.session_cookie, sended_smscode);

			std::error_code stdec;
			node_js_code_sender.wait_for(std::chrono::milliseconds(12), stdec);
			if (!node_js_code_sender.running())
				node_js_code_sender.join();

			co_return ret;
		}

		// verify the user input verify_code against verify_session
		boost::asio::awaitable<bool> verify_verify_code(std::string verify_code, verify_session verify_session_tag)
		{
			co_return verify_code_stor.get(verify_session_access::as_string(verify_session_tag)) == verify_code;
		}

		boost::asio::io_context& io;

		utility::timedmap<std::string, std::string> verify_code_stor;
	};

	// send verifycode, returns verify session.
	boost::asio::awaitable<verify_session> verifycode::send_verify_code(std::string telephone, boost::system::error_code& ec)
	{
		return impl().send_verify_code(telephone, ec);
	}

	boost::asio::awaitable<verify_session> verifycode::send_verify_code(std::string telephone)
	{
		boost::system::error_code ec;
		auto ret = co_await impl().send_verify_code(telephone, ec);

		if (ec)
		{
			throw boost::system::system_error(ec);
		}

		co_return ret;
	}

	// verify the user input verify_code against verify_session
	boost::asio::awaitable<bool> verifycode::verify_verify_code(std::string verify_code, verify_session verify_session_tag)
	{
		return impl().verify_verify_code(verify_code, verify_session_tag);
	}

	verifycode::verifycode(boost::asio::io_context& io)
	{
		static_assert(sizeof(obj_stor) >= sizeof(verifycode_impl));
		std::construct_at(reinterpret_cast<verifycode_impl*>(obj_stor.data()), io);
	}

	verifycode::~verifycode()
	{
		std::destroy_at(reinterpret_cast<verifycode_impl*>(obj_stor.data()));
	}

	const verifycode_impl& verifycode::impl() const
	{
		return *reinterpret_cast<const verifycode_impl*>(obj_stor.data());
	}

	verifycode_impl& verifycode::impl()
	{
		return *reinterpret_cast<verifycode_impl*>(obj_stor.data());
	}

}
