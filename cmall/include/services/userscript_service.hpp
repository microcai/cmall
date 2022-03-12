
#pragma once

#include <string>
#include <boost/asio.hpp>

namespace services
{
	struct userscript_impl;
	class userscript
	{
	public:
		userscript(boost::asio::io_context& io);
		~userscript();

		// get userscript url.
		boost::asio::awaitable<std::string> run_script(std::string_view script_file_content, std::vector<std::string> script_arguments);
		// query userscript success.

	private:
		const userscript_impl& impl() const;
		userscript_impl& impl();

		std::array<char, 512> obj_stor;

	};
}
