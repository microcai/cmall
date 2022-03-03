
#pragma once

#include <git2/types.h>
#include <git2/buffer.h>

#include <boost/filesystem.hpp>
#include <boost/noncopyable.hpp>

namespace gitpp {

	namespace exception{

		struct git2_exception : public std::exception
		{
			virtual const char* what() const _NOEXCEPT { return "libgit2 exception";}
		};

		struct not_repo : public git2_exception
		{
	      virtual const char* what() const _NOEXCEPT { return "not a git repo";}
		};

		struct resolve_failed : public git2_exception
		{
			virtual const char* what() const _NOEXCEPT { return "reference invalid";}
		};
	}

	class oid
	{
		git_oid oid_;
	public:
		const git_oid* operator & () const
		{
			return &oid_;
		}

		git_oid* operator & ()
		{
			return &oid_;
		}

		oid(const git_oid* git_oid_)
		{
			memcpy(&oid_, git_oid_, sizeof (oid_));
		}
	};

	class reference
	{
		git_reference* ref = nullptr;
		bool owned = false;
	public:
		explicit reference(git_reference* ref);
		explicit reference(const git_reference* ref);
		~reference();

		reference(reference&&);
		reference(const reference&);

	public:
		git_reference_t type() const;
		oid target() const;
		reference resolve() const;
	};

	struct buf : boost::noncopyable
	{
		git_buf buf_;

		git_buf * operator & ()
		{
			return &buf_;
		}

		buf();
		~buf();
	};

	class tree
	{
		git_tree* tree_;
	public:
		tree(git_tree*);

	};

	class repo
	{
		git_repository* repo_;
	public:
		explicit repo(git_repository*) noexcept;
		repo(boost::filesystem::path repo_dir);                   // throws not_repo
		~repo() noexcept;

		reference head() const;

		git_repository* native_handle() { return repo_;}
		const git_repository* native_handle() const { return repo_;}

		tree get_tree(oid);

	public:
		bool is_bare() const noexcept;

	};

	bool is_bare_repo(boost::filesystem::path);
	bool init_bare_repo(boost::filesystem::path repo_path);

}
