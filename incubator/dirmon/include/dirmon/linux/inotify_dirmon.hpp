
#pragma once

#include <boost/asio.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/awaitable.hpp>
#include "../change_notify.hpp"

#include <sys/inotify.h>

namespace dirmon {
	class inotify_dirmon
	{
	public:
		template<typename ExecutionContext>
		inotify_dirmon(ExecutionContext&& e, std::string dirname)
			: m_inotify_handle(std::forward<ExecutionContext>(e), inotify_init1(IN_CLOEXEC))
		{
            inotify_add_watch(m_inotify_handle.native_handle(), dirname.c_str(), IN_CLOSE_WRITE|IN_MODIFY|IN_OPEN);
        }

		boost::asio::awaitable<std::vector<dir_change_notify>> async_wait_dirchange()
		{
			std::vector<dir_change_notify> ret;
			std::array<char, 4096> buf;
			auto bytes_transferred = co_await m_inotify_handle.async_read_some(boost::asio::buffer(buf), boost::asio::use_awaitable);
            decltype(bytes_transferred) pos = 0;

			if (bytes_transferred >= sizeof(inotify_event))
			{
				for (inotify_event* file_notify_info = reinterpret_cast<inotify_event*>(buf.data()); pos < bytes_transferred;
                    pos += sizeof(inotify_event) + file_notify_info->len,
					file_notify_info = reinterpret_cast<inotify_event*>(reinterpret_cast<char*>(file_notify_info) + sizeof(inotify_event)+file_notify_info->len)
				)
				{
					if (file_notify_info->len > 0)
					{
						dir_change_notify c;
						c.file_name = std::string(file_notify_info->name, file_notify_info->len -1);
						ret.push_back(c);
					}
				}
			}

			co_return ret;
		}

		void close(boost::system::error_code ec)
		{
			m_inotify_handle.close(ec);
		}

		boost::asio::posix::stream_descriptor m_inotify_handle;
	};
}
