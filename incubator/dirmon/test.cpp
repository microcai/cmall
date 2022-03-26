
#include "dirmon/dirmon.hpp"
#include <iostream>

boost::asio::awaitable<int> co_main(int argc, char** argv, boost::asio::io_context& io)
{
	dirmon::dirmon monitor(io, ".");

	auto changed_list = co_await monitor.async_wait_dirchange();

	for (auto& c : changed_list)
	{
		std::cout << "got change. filename: " << c.file_name << "\n";
	}

	co_return 0;
}

int main(int argc, char** argv)
{
	int main_return;
	boost::asio::io_context io;

	boost::asio::co_spawn(io, co_main(argc, argv, io), [&](std::exception_ptr e, int ret) {
		if (e)
			std::rethrow_exception(e);
		main_return = ret;
		io.stop();
		});
	io.run();
	return main_return;
}
