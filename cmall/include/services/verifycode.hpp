
#pragma once

#include <memory>
#include <boost/asio.hpp>

namespace services
{
	struct verifycode_impl;
	struct verify_session
	{
	private:
		std::string session_cookie;
		friend class verifycode_impl;
	};

	class verifycode
	{
	public:
		verifycode(boost::asio::io_context& io);
		~verifycode();

		// send verifycode, returns verify session.
		boost::asio::awaitable<verify_session> send_verify_code(std::string telephone, boost::system::error_code& ec);

		// verify the user input verify_code against verify_session
		boost::asio::awaitable<bool> verify_verify_code(std::string verify_code, verify_session verify_session_tag);

	private:
		const verifycode_impl& impl() const;
		verifycode_impl& impl();

		std::array<char, 512> obj_stor;



	};
}
