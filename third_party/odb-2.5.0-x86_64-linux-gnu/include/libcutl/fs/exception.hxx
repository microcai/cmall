// file      : libcutl/fs/exception.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_FS_EXCEPTION_HXX
#define LIBCUTL_FS_EXCEPTION_HXX

#include <libcutl/exception.hxx>

#include <libcutl/export.hxx>

namespace cutl
{
  namespace fs
  {
    struct LIBCUTL_EXPORT error: exception
    {
      error (int code): code_ (code) {}

      // Error code (errno).
      //
      int
      code () const
      {
        return code_;
      }

      virtual char const*
      what () const noexcept;

    private:
      int code_;
    };
  }
}

#endif // LIBCUTL_FS_EXCEPTION_HXX
