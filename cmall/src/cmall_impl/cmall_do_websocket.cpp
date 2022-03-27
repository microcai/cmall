#include "stdafx.hpp"

#include "cmall/cmall.hpp"
#include "cmall/js_util.hpp"

#include "cmall/conversion.hpp"
#include "httpd/http_misc_helper.hpp"
#include "services/repo_products.hpp"
#include "services/search_service.hpp"

awaitable<void> cmall::cmall_service::do_ws_read(size_t connection_id, client_connection_ptr connection_ptr)
{
	while (!m_abort)
	{
		boost::beast::multi_buffer buffer{ 4 * 1024 * 1024 }; // max multi_buffer size 4M.
		co_await connection_ptr->ws_client->ws_stream_.async_read(buffer, use_awaitable);

		auto body = boost::beast::buffers_to_string(buffer.data());

		boost::system::error_code ec;
		auto jv = boost::json::parse(body, ec, {}, { 64, false, false, true });

		if (ec || !jv.is_object())
		{
			// 这里直接　co_return, 连接会关闭. 因为用户发来的数据连 json 都不是.
			// 对这种垃圾客户端，直接暴力断开不搭理.
			co_return;
		}

		maybe_jsonrpc_request_t maybe_req = boost::json::value_to<maybe_jsonrpc_request_t>(jv);
		if (!maybe_req.has_value())
		{
			// 格式不对.
			boost::json::object reply_message;
			if (jv.as_object().contains("id"))
				reply_message["id"] = jv.at("id");
			reply_message["error"] = { { "code", -32600 }, { "message", "Invalid Request" } };
			co_await websocket_write(connection_ptr, jsutil::json_to_string(reply_message)).async_wait(use_awaitable);
			continue;
		}

		auto req	= maybe_req.value();
		auto method = req.method;
		auto params = req.params;

		client_connection& this_client = *connection_ptr;

		if (!this_client.session_info)
		{
			if (method != "recover_session")
			{
				throw boost::system::system_error(cmall::error::session_needed);
			}
			boost::json::object replay_message;
			// 未有 session 前， 先不并发处理 request，避免 客户端恶意并发 recover_session 把程序挂掉
			try
			{
				replay_message = co_await handle_jsonrpc_call(connection_ptr, method, params);
			}
			catch (boost::system::system_error& e)
			{
				replay_message["error"] = { { "code", e.code().value() }, { "message", e.code().message() } };
			}
			catch (std::exception& e)
			{
				LOG_ERR << e.what();
				replay_message["error"] = { { "code", 502 }, { "message", "internal server error" } };
			}
			if (jv.as_object().contains("id"))
				replay_message.insert_or_assign("id", jv.at("id"));
			co_await websocket_write(connection_ptr, jsutil::json_to_string(replay_message)).async_wait(use_awaitable);
			continue;
		}

		// 每个请求都单开线程处理
		boost::asio::co_spawn(
			connection_ptr->get_executor(),
			[this, connection_ptr, method, params, jv]() -> awaitable<void>
			{
				boost::json::object replay_message;
				try
				{
					replay_message = co_await handle_jsonrpc_call(connection_ptr, method, params);
				}
				catch (boost::system::system_error& e)
				{
					replay_message["error"] = { { "code", e.code().value() }, { "message", e.code().message() } };
				}
				catch (std::exception& e)
				{
					LOG_ERR << e.what();
					replay_message["error"] = { { "code", 502 }, { "message", "internal server error" } };
				}
				// 每个请求都单开线程处理, 因此客户端收到的应答是乱序的,
				// 这就是 jsonrpc 里 id 字段的重要意义.
				// 将 id 字段原原本本的还回去, 客户端就可以根据 返回的 id 找到原来发的请求
				if (jv.as_object().contains("id"))
					replay_message.insert_or_assign("id", jv.at("id"));
				co_await websocket_write(connection_ptr, jsutil::json_to_string(replay_message))
					.async_wait(use_awaitable);
			},
			boost::asio::detached);
	}
}

awaitable<void> cmall::cmall_service::do_ws_write(size_t connection_id, client_connection_ptr connection_ptr)
{
	auto& message_deque = connection_ptr->ws_client->message_channel;
	auto& ws			= connection_ptr->ws_client->ws_stream_;
	auto& t = connection_ptr->ws_client->message_channel_timer;

	using namespace boost::asio::experimental::awaitable_operators;

	while (!m_abort)
	{
		t.expires_from_now(std::chrono::seconds(15));
		std::variant<std::monostate, std::string> awaited_result
			= co_await (t.async_wait(use_awaitable) || message_deque.async_receive(use_awaitable));
		if (awaited_result.index() == 0)
		{
			LOG_DBG << "coro: do_ws_write: [" << connection_id << "], send ping to client";
			co_await ws.async_ping("", use_awaitable); // timed out
		}
		else
		{
			auto message = std::get<1>(awaited_result);
			if (message.empty())
				co_return;
			co_await ws.async_write(boost::asio::buffer(message), use_awaitable);
		}
	}
}

promise<void(std::exception_ptr)> cmall::cmall_service::websocket_write(
	client_connection_ptr clientptr, std::string message)
{
	// 本来 co_await co_spawn 就好，但是 gcc 上有 bug, 会崩
	// 所以返回 promise 然后在 promise 上 wait 就不会崩了.
	// 那为啥要在外面await 而不是这里 await 掉，是发现了新的情况.
	// 循环 for (c : all_client) promises.push_back(websocket_write(c, 'msg')); 的时候， 可以收集 promise
	// 然后 wait_all_promise(promises);
	if (clientptr->ws_client)
	{
		return boost::asio::co_spawn(
			clientptr->get_executor(),
			[clientptr, message = std::move(message)]() mutable -> awaitable<void>
			{
				clientptr->ws_client->message_channel.try_send(boost::system::error_code(), message);
				co_return;
			},
			use_promise);
	}
	throw std::runtime_error("write ws message on non-ws socket");
}
