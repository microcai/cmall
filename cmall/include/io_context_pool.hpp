//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#pragma once

#include <vector>
#include <boost/asio.hpp>

/// A pool of io_context objects.
class io_context_pool
{
	// c++11 noncopyable.
	io_context_pool(const io_context_pool&) = delete;
	io_context_pool& operator=(const io_context_pool&) = delete;

public:
	/// Construct the io_context pool.
	explicit io_context_pool(std::size_t pool_size);

	/// Run all io_context objects in the pool.
	void run();

	/// Stop all io_context objects in the pool.
	void stop();

	/// Get an io_context to use for a client.
	boost::asio::io_context& get_io_context();

	boost::asio::io_context& get_io_context(std::size_t index);

	/// Get main_io_context_ to use.
	boost::asio::io_context& server_io_context();

	/// Get pool size.
	std::size_t pool_size() const;

	void notify_fork(boost::asio::execution_context::fork_event event);


private:
	using io_context_ptr = std::shared_ptr<boost::asio::io_context>;
	using work_ptr = std::shared_ptr<boost::asio::io_context::work>;

	// main io_context that used to run the main logic
	boost::asio::io_context main_io_context_;

	/// The pool of io_contexts for client sockets
	std::vector<io_context_ptr> io_contexts_;

	/// The work that keeps the io_contexts running.
	std::vector<work_ptr> work_;

	std::size_t next_io_context_;
};
