
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <iostream>

#include "utils/httpc.hpp"

int main(int argc, char* argv[])
{
	boost::asio::io_context ioc;
	std::string uri = argv[1];

	httpc::request_options_t opts{ httpc::http::verb::get, uri };
	boost::asio::co_spawn(ioc,
		httpc::request(std::move(opts)),
		[](std::exception_ptr e, httpc::response_t resp)
		{
			try
			{
				if (e)
					std::rethrow_exception(e);
			}
			catch (std::exception& e)
			{
				std::cout << "exception: " << e.what() << std::endl;
			}
			if (resp.err.has_value())
				std::cout << "err: " << resp.err.value() << std::endl;
			std::cout << "code: " << resp.code << std::endl;
			std::cout << "content-type: " << resp.content_type << std::endl;
			std::cout << "body: " << resp.body << std::endl;
		});
	return ioc.run();
}
