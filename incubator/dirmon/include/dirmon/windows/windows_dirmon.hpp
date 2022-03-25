
#pragma once

#include <boost/asio.hpp>
#include <boost/asio/windows/overlapped_handle.hpp>

class windows_dirmon
{
public:
	windows_dirmon()
	{}


	boost::asio::windows::overlapped_handle iocp;

};
