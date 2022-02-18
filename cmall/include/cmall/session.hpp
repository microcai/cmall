#pragma once

#include "boost/asio/associated_executor.hpp"
#include "boost/asio/spawn.hpp"
#include "boost/asio/steady_timer.hpp"
#include "boost/beast/core/buffers_to_string.hpp"
#include "boost/beast/core/error.hpp"
#include "boost/beast/core/multi_buffer.hpp"
#include "boost/beast/core/tcp_stream.hpp"
#include "boost/beast/websocket/error.hpp"
#include "boost/beast/websocket/stream.hpp"
#include "boost/signals2/connection.hpp"
#include "logging.hpp"
#include "boost/system.hpp"
#include "boost/beast/websocket.hpp"
#include "boost/json.hpp"
#include "boost/signals2.hpp"
#include "misc.hpp"

namespace cmall {
	namespace beast = boost::beast;
	// namespace http = beast::http;
	namespace websocket = beast::websocket;
	namespace net = boost::asio;
	using websocket_stream = websocket::stream<beast::tcp_stream>;

	struct request_context {
		boost::json::value payload_;
		std::uint64_t sid_;
		boost::asio::yield_context& yield_;
	};
	using message_handler = void (const request_context&);
	using sig_on_message_t = boost::signals2::signal<message_handler>;

	// template<typename Stream>
	// class jsonrpc {

	// };

	class session {
		std::uint64_t sid_;
		websocket::stream<beast::tcp_stream> ws_;
		bool stop_;
		std::deque<std::string> queue_;

	// public:
		sig_on_message_t got_message;
		boost::signals2::connection sig_conn_;

	public:
		session(std::uint64_t id, beast::tcp_stream&& stream)
			: sid_(id)
			, ws_(std::move(stream))
			, stop_(false)
		{
			ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
		}
		session(std::uint64_t id, websocket_stream&& stream)
			: sid_(id)
			, ws_(std::move(stream))
			, stop_(false)
		{
			ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
		}

		void start() {
			stop_ = false;
			auto executor = ws_.get_executor();
			boost::asio::spawn(executor, [this](boost::asio::yield_context yield) {
				do_read(yield);
			});
			boost::asio::spawn(executor, [this](boost::asio::yield_context yield) {
				do_write(yield);
			});
		}

		void stop() {
			stop_ = true;
			if (sig_conn_.connected()) {
				sig_conn_.disconnect();
			}
		}

		void register_hander(std::function<message_handler>&& handler) {
			sig_conn_ = got_message.connect(handler);
		}

		void reply(const std::string& res) {
			queue_.emplace_back(res);
		}

private:
		void do_read(boost::asio::yield_context yield) {
			beast::error_code ec;
			while (!stop_) {
				beast::multi_buffer buf{ 4 * 1024 * 1024}; // 4M
				ws_.async_read(buf, yield[ec]);
				if (ec == websocket::error::closed) {
					LOG_WARN << "do_read, id: " << sid_ << ", ws closed";
					break;
				}
				if (ec) {
					LOG_ERR << "do_read, id: " << sid_ << ", async_read error: " << ec.message();
					break;
				}

				auto data = beast::buffers_to_string(buf.data());
				auto payload = boost::json::parse(data, ec);
				if (ec) {
					LOG_ERR << "invalid data: " << data << ", json parse error: " << ec.message() << ", id: " << sid_;
					break;
				}

				if (!payload.is_object()) {
					LOG_ERR << "invalid payload: " << data;
					break;
				}


				auto version = json_as_string(json_accessor(payload.as_object()).get("jsonrpc", ""));
				if (version != "2.0") {
					LOG_WARN << "invalid jsonrpc version: " << version << ", id:" << sid_;
					break;
				}
				auto method = json_as_string(json_accessor(payload.as_object()).get("method", ""));
				if (method.empty()) {
					LOG_WARN << "invalid jsonrpc call: no method, data: " << data << ", id: " << sid_;
					break;
				}

				request_context ctx = { payload, sid_, yield };
				got_message(ctx);
			}

			// do cleanup
		}

		void do_write(boost::asio::yield_context yield) {
			beast::error_code ec;
			boost::asio::steady_timer timer{boost::asio::get_associated_executor(yield)};
			while (!stop_) {
				if (queue_.empty()) {
					// wait 1s
					using namespace std::chrono_literals;

					timer.expires_after(1s);
					timer.async_wait(yield[ec]);
					// ignore error
				} else {
					while (!queue_.empty()) {
						const auto& msg = queue_.front();
						ws_.async_write(net::buffer(msg), yield[ec]);
						if (ec) {
							LOG_ERR << "session:" << sid_ << " async_write error: " << ec.message();
						}
						queue_.pop_front();
						if (stop_) break;
					}
				}
			}
		}
	};
	using session_ptr = std::shared_ptr<session>;
};
