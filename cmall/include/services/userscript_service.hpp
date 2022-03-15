
#pragma once

#include <string>
#include <vector>
#include <map>
#include <boost/asio.hpp>

namespace services
{
	struct userscript_impl;
	class userscript
	{
	public:
		userscript(boost::asio::io_context& io);
		~userscript();

		boost::asio::awaitable<std::string> run_script(std::string_view script_file_content, std::vector<std::string> script_arguments);

		// http_request_body 作为 stdin 输入. http header 在 scriptenv 里
		// get/post 在 script_arguments 上 --metohd POST
		boost::asio::awaitable<std::string> run_script(
			std::string_view script_file_content,
			std::string_view http_request_body,
			std::map<std::string, std::string> scriptenv,
			std::vector<std::string> script_arguments);

	private:
		const userscript_impl& impl() const;
		userscript_impl& impl();

		std::array<char, 512> obj_stor;

	};
}
