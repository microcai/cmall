
#pragma once

#include <boost/asio.hpp>
#include <boost/asio/windows/overlapped_handle.hpp>
#include "win_dirchange_read_handle.hpp"
#include "../change_notify.hpp"

namespace dirmon {
	class windows_dirmon
	{

		struct async_wait_dirchange_op
		{
			template<typename Handler>
			void operator()(Handler&& handler)
			{

			}

			async_wait_dirchange_op(windows_dirmon* parent) : parent_(parent) {}
			windows_dirmon* parent_;
		};

		friend struct async_wait_dirchange_op;
	public:
		template<typename ExecutionContext>
		windows_dirmon(ExecutionContext&& e)
			: m_dirhandle(e)
		{}

		template<typename Handler>
		auto async_wait_dirchange(Handler&& h)
		{
			boost::asio::async_initiate<Handler, void(boost::system::error_code, std::vector<dir_change_notify>)>(
				async_wait_dirchange_op(this), h);
		}

		detail::win_dirchange_read_handle m_dirhandle;
	};
}