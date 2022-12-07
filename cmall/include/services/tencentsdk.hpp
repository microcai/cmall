
#pragma once

#include <memory>
#include <boost/asio.hpp>

using boost::asio::awaitable;

namespace services
{
	struct tencentsdk_impl;
	class tencentsdk
	{
	public:
		tencentsdk(boost::asio::io_context& io,
			const std::string& secret_id,
			const std::string& secret_key);
		~tencentsdk();

		awaitable<bool> group_create(std::string gid, std::string gname);
		awaitable<bool> group_add_person(std::string gid, std::string pid, std::string pname, std::string input);
		// 查询人员归属. GetPersonGroupInfo
		awaitable<std::vector<std::string>> get_person_group(std::string pid); 
		awaitable<bool> person_add_face(std::string pid, std::string inputs);
		awaitable<std::string> person_search_by_face(std::string gid, std::string input);

	private:
		const tencentsdk_impl& impl() const;
		tencentsdk_impl& impl();

		std::array<char, 512> obj_stor;
	};
}
