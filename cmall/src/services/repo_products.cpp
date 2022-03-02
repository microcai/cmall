
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/process.hpp>

#include "services/repo_products.hpp"
#include "utils/logging.hpp"

extern "C" {
#include <git2.h>
}

namespace services
{
	struct repo_products_impl
	{
		repo_products_impl(boost::asio::io_context& io, boost::filesystem::path repo_path)
			: io(io)
			, repo_path(repo_path)
		{


		}

		boost::asio::io_context& io;
		boost::filesystem::path repo_path;
	};

	repo_products::repo_products(boost::asio::io_context& io, boost::filesystem::path repo_path)
	{
		git_libgit2_init();
		static_assert(sizeof(obj_stor) >= sizeof(repo_products_impl));
		std::construct_at(reinterpret_cast<repo_products_impl*>(obj_stor.data()), io, repo_path);
	}

	repo_products::~repo_products()
	{
		std::destroy_at(reinterpret_cast<repo_products_impl*>(obj_stor.data()));
		git_libgit2_shutdown();
	}

	const repo_products_impl& repo_products::impl() const
	{
		return *reinterpret_cast<const repo_products_impl*>(obj_stor.data());
	}

	repo_products_impl& repo_products::impl()
	{
		return *reinterpret_cast<repo_products_impl*>(obj_stor.data());
	}




}
