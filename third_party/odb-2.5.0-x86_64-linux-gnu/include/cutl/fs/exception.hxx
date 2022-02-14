// file      : cutl/fs/exception.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef CUTL_FS_EXCEPTION_HXX
#define CUTL_FS_EXCEPTION_HXX

#include <cutl/exception.hxx>

#include <cutl/details/config.hxx>

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
      what () const LIBCUTL_NOTHROW_NOEXCEPT;

    private:
      int code_;
    };
  }
}

#endif // CUTL_FS_EXCEPTION_HXX
