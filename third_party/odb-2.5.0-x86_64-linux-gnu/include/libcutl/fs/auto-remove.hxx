// file      : libcutl/fs/auto-remove.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_FS_AUTO_REMOVE_HXX
#define LIBCUTL_FS_AUTO_REMOVE_HXX

#include <vector>
#include <utility>

#include <libcutl/fs/path.hxx>
#include <libcutl/fs/exception.hxx>

#include <libcutl/export.hxx>

namespace cutl
{
  namespace fs
  {
    // Remove a file or an empty directory on destruction unless canceled.
    //
    struct LIBCUTL_EXPORT auto_remove
    {
      auto_remove ()
          : canceled_ (true)
      {
      }

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

      // Movable-only type. Move-assignment cancels the lhs object.
      //
      auto_remove (auto_remove&& x)
          : path_ (std::move (x.path_)), canceled_ (x.canceled_)
      {
        x.canceled_ = true;
      }

      auto_remove&
      operator= (auto_remove&& x)
      {
        if (this != &x)
        {
          path_ = std::move (x.path_);
          canceled_ = x.canceled_;
          x.canceled_ = true;
        }

        return *this;
      }

      auto_remove (auto_remove const&) = delete;
      auto_remove& operator= (auto_remove const&) = delete;

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

#endif // LIBCUTL_FS_AUTO_REMOVE_HXX
