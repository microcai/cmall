
#pragma once

#include <boost/asio.hpp>
#include <boost/asio/windows/overlapped_handle.hpp>

namespace dirmon::detail {

	template<typename IoExecutor = boost::asio::any_io_executor>
	class basic_win_dirchange_read_handle : public boost::asio::windows::basic_overlapped_handle<IoExecutor>
	{
	public:
		template<typename ExecutionContext>
		basic_win_dirchange_read_handle(ExecutionContext&& context, std::string dirname)
			: boost::asio::windows::basic_overlapped_handle<IoExecutor>(std::forward(context), CreateFileW(boost::nowide::widen(dirname).c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL))
			, iocp_service_(boost::asio::use_service<boost::asio::detail::win_iocp_io_context>(context))
		{
		}

		~basic_win_dirchange_read_handle(){}

		template<typename MutableBufferSequence, typename Handler>
		auto async_read_some(const MutableBufferSequence& b, Handler&& h)
		{
			return boost::asio::async_initiate<Handler, void(boost::system::error_code, std::size_t)>([this](auto&& handler, auto&& b) mutable
				{
					this->async_read_some_impl(b, std::move(handler));
				}, h, b);
		}

	private:

		template<typename MutableBufferSequence, typename Handler>
		auto async_read_some_impl(const MutableBufferSequence& buffers, Handler&& handler)
		{
			auto slot = boost::asio::get_associated_cancellation_slot(handler);
			// Allocate and construct an operation to wrap the handler.
			typedef boost::asio::detail::win_iocp_handle_read_op<
				MutableBufferSequence, Handler, IoExecutor> op;
			typename op::ptr p = { boost::asio::detail::addressof(handler),
			  op::ptr::allocate(handler), 0 };
			boost::asio::detail::operation* o = p.p = new (p.v) op(buffers, handler, this->get_executor());

			// Optionally register for per-operation cancellation.
			if (slot.is_connected())
				o = &slot.template emplace<boost::asio::detail::iocp_op_cancellation>(impl.handle_, o);

			start_read_op(boost::asio::detail::buffer_sequence_adapter<boost::asio::mutable_buffer,
				MutableBufferSequence>::first(buffers), o);
			p.v = p.p = 0;
		}

	private:

		void start_read_op(const boost::asio::mutable_buffer& buffer, boost::asio::detail::operation* op)
		{
			DWORD bytes_transferred = 0;
			op->Offset = 0;
			op->OffsetHigh = 0;
			BOOL ok = ::ReadDirectoryChangesW(this->native_handle(), buffer.data(),
				static_cast<DWORD>(buffer.size()), TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME
					, &bytes_transferred, op, NULL);
			DWORD last_error = ::GetLastError();
			if (!ok && last_error != ERROR_IO_PENDING
				&& last_error != ERROR_MORE_DATA)
			{
				iocp_service_.on_completion(op, last_error, bytes_transferred);
			}
			else
			{
				iocp_service_.on_pending(op);
			}
		}

	protected:
		boost::asio::detail::win_iocp_io_context& iocp_service_;
	};

	using win_dirchange_read_handle = basic_win_dirchange_read_handle<boost::asio::any_io_executor>;
} // namespace dirmon::detail
