
#pragma once

#include <memory>
#include <boost/asio.hpp>

using boost::asio::awaitable;

namespace services
{
	struct verify_session_access;
	struct verifycode_impl;
	struct verify_session
	{
	private:
		std::string session_cookie;
		friend struct verifycode_impl;
		friend struct verify_session_access;
	};

	struct verify_session_access
	{
		static std::string as_string(const verify_session& s)
		{
			return s.session_cookie;
		}

		static verify_session from_string(std::string s)
		{
			verify_session r;
			r.session_cookie = s;
			return r;
		}
	};

	class verifycode
	{
	public:
		verifycode(boost::asio::io_context& io);
		~verifycode();

		// send verifycode, returns verify session.
		awaitable<verify_session> send_verify_code(std::string telephone);
		awaitable<verify_session> send_verify_code(std::string telephone, boost::system::error_code& ec);

		// verify the user input verify_code against verify_session
		awaitable<bool> verify_verify_code(std::string verify_code, verify_session verify_session_tag);

	private:
		const verifycode_impl& impl() const;
		verifycode_impl& impl();

		std::array<char, 512> obj_stor;


	};
}
