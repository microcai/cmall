
#include "services/verifycode.hpp"

namespace services
{
	struct verifycode_impl
	{
		verifycode_impl(boost::asio::io_context& io)
			: io(io)
		{}

		// send verifycode, returns verify session.
		boost::asio::awaitable<verify_session> send_verify_code(std::string telephone, boost::system::error_code& ec)
		{
			verify_session ret;

			ret.session_cookie = "fake_test:" + telephone;

			co_return ret;
		}

		// verify the user input verify_code against verify_session
		boost::asio::awaitable<bool> verify_verify_code(std::string verify_code, verify_session verify_session_tag)
		{
			co_return true;
		}

		boost::asio::io_context& io;
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
