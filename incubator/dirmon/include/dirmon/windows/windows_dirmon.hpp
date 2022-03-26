
#pragma once

#include <boost/asio.hpp>
#include <boost/asio/windows/overlapped_handle.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/nowide/convert.hpp>
#include "win_dirchange_read_handle.hpp"
#include "../change_notify.hpp"

namespace dirmon {
	class windows_dirmon
	{
	public:
		template<typename ExecutionContext>
		windows_dirmon(ExecutionContext& e)
			: m_dirhandle(e)
		{}

		boost::asio::awaitable<std::vector<dir_change_notify>> async_wait_dirchange()
		{
			std::vector<dir_change_notify> ret;
			std::array<char, 4096> buf;
			auto bytes_transferred = co_await m_dirhandle.async_read_some(boost::asio::buffer(buf), boost::asio::use_awaitable);

			FILE_NOTIFY_INFORMATION* file_notify_info = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(buf.data());

			while (file_notify_info->NextEntryOffset != 0)
			{
				dir_change_notify c;

				c.file_name = boost::nowide::narrow(file_notify_info->FileName, file_notify_info->FileNameLength);

				ret.push_back(c);
			}

			co_return ret;
		}

		detail::win_dirchange_read_handle m_dirhandle;
	};
}