
#pragma once

#include <memory>
#include <filesystem>
#include <chrono>
#include <boost/asio/awaitable.hpp>

struct persist_map_impl;
class persist_map
{
public:
	persist_map(std::filesystem::path persist_file);
	~persist_map();

	boost::asio::awaitable<std::string> get(std::string_view key) const;
	boost::asio::awaitable<void> put(std::string_view key, std::string value, std::chrono::duration<int> lifetime = std::chrono::seconds(86400 * 30));

private:
	std::shared_ptr<persist_map_impl> impl;
};

