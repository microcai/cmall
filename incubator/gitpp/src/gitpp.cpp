
#include "gitpp/gitpp.hpp"

#include <git2.h>

struct auto_init_libgit2 {
	auto_init_libgit2() {
		git_libgit2_init();
	};
	~auto_init_libgit2() {
		git_libgit2_shutdown();
	}
};

static auto_init_libgit2 ensure_libgit2_initialized;

gitpp::repo::repo(git_repository* repo_) noexcept
	: repo_(repo_)
{
}

gitpp::repo::repo(boost::filesystem::path repo_dir)
	: repo_(nullptr)
{
	if (0 != git_repository_open_ext(&repo_, repo_dir.c_str(), GIT_REPOSITORY_OPEN_NO_SEARCH, nullptr))
	{
		throw exception::not_repo();
	}
}

gitpp::repo::~repo() noexcept
{
	if (repo_)
		git_repository_free(repo_);
}

bool gitpp::repo::is_bare() const noexcept
{
	return git_repository_is_bare(repo_);
}

bool gitpp::is_bare_repo(boost::filesystem::path dir)
{
	try
	{
		repo tmp(dir);
		return tmp.is_bare();
	}
	catch (exception::not_repo&)
	{}
	return false;
}

gitpp::reference gitpp::repo::head() const
{
	git_reference* out_ref = nullptr;
	git_repository_head(&out_ref, repo_);

	return reference(out_ref);
}

gitpp::tree gitpp::repo::get_tree(gitpp::oid tree_oid)
{
	git_tree* ctree = nullptr;
	git_tree_lookup(&ctree, repo_, & tree_oid);
	return tree(ctree);
}

bool gitpp::init_bare_repo(boost::filesystem::path repo_path)
{
	git_repository * repo = nullptr;

	int lg_ret = git_repository_init(&repo, repo_path.c_str(), true);
	git_repository_free(repo);

	return lg_ret == 0;

}

gitpp::reference::reference(git_reference* ref)
	: ref(ref), owned(true)
{
}

gitpp::reference::reference(const git_reference* ref)
	: ref(const_cast<git_reference*>(ref)), owned(false)
{
}

gitpp::reference::reference(const gitpp::reference& other)
	: ref(nullptr), owned(true)
{
	git_reference_dup(&ref, other.ref);
}

gitpp::reference::reference(gitpp::reference&& other)
	: ref(other.ref), owned(other.owned)
{
	other.owned = false;
}

gitpp::reference::~reference()
{
	if (owned)
		git_reference_free(ref);
}

git_reference_t gitpp::reference::type() const
{
	return git_reference_type(ref);
}

gitpp::reference gitpp::reference::resolve() const
{
	git_reference* out = nullptr;
	if (0 == git_reference_resolve(&out, ref))
		return reference(out);
	throw exception::resolve_failed();
}

gitpp::oid gitpp::reference::target() const
{
	if (type() == GIT_REFERENCE_DIRECT)
	{
		const git_oid* tgt = git_reference_target(ref);
		if (tgt == nullptr)
			throw exception::resolve_failed{};
		return oid(tgt);
	}
	return resolve().target();
}

gitpp::buf::buf()
{
	memset(&buf_, 0, sizeof buf_);
}

gitpp::buf::~buf()
{
	git_buf_dispose(&buf_);
}
