
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
		static git_repository* open_bare(const char* bare_dir)
		{
			git_repository * out = nullptr;
			git_repository_open_bare(&out, bare_dir);
			return out;
		}
		repo_products_impl(boost::asio::io_context& io, boost::filesystem::path repo_path)
			: io(io)
			, repo_path(repo_path)
			, git_repo(open_bare(repo_path.c_str()), &git_repository_free)
		{
			git_reference* headref = nullptr;
			git_repository_head(&headref, git_repo.get());
			git_tree* tree = nullptr;
			git_tree_lookup(&tree, git_repo.get(), git_reference_target(headref));
			repo_fs_tree.reset(tree, git_tree_free);
		}

		boost::asio::io_context& io;
		boost::filesystem::path repo_path;
		std::unique_ptr<git_repository, decltype(&git_repository_free)> git_repo;
		std::shared_ptr<git_tree> repo_fs_tree;
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

	bool repo_products::init_bare_repo(boost::filesystem::path repo_path)
	{
		git_libgit2_init();

		git_repository * repo = nullptr;

		int lg_ret = git_repository_init(&repo, repo_path.c_str(), true);
		git_repository_free(repo);
		git_libgit2_shutdown();

		return lg_ret == 0;
	}

}
