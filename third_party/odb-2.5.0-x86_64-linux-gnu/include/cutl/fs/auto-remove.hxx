// file      : cutl/fs/auto-remove.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef CUTL_FS_AUTO_REMOVE_HXX
#define CUTL_FS_AUTO_REMOVE_HXX

#include <vector>

#include <cutl/fs/path.hxx>
#include <cutl/fs/exception.hxx>

#include <cutl/details/export.hxx>

namespace cutl
{
  namespace fs
  {
    // Remove a file or an empty directory on destruction unless canceled.
    //
    struct LIBCUTL_EXPORT auto_remove
    {
      explicit
      auto_remove (path const& p)
          : path_ (p), canceled_ (false)
      {
      }

      ~auto_remove ();

      void
      cancel ()
      {
        canceled_ = true;
      }

    private:
      auto_remove (auto_remove const&);

      auto_remove&
      operator= (auto_remove const&);

    private:
      path path_;
      bool canceled_;
    };

    // Remove a list of file or aempty directories on destruction unless
    // canceled.
    //
    struct LIBCUTL_EXPORT auto_removes
    {
      auto_removes (): canceled_ (false) {}
      ~auto_removes ();

      void
      add (path const& p)
      {
        paths_.push_back (p);
      }

      void
      cancel ()
      {
        canceled_ = true;
      }

    private:
      auto_removes (auto_removes const&);

      auto_removes&
      operator= (auto_removes const&);

    private:
      typedef std::vector<path> paths;

      paths paths_;
      bool canceled_;
    };
  }
}

#endif // CUTL_FS_AUTO_REMOVE_HXX
