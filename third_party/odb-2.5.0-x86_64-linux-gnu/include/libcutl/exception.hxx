// file      : libcutl/exception.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_EXCEPTION_HXX
#define LIBCUTL_EXCEPTION_HXX

#include <exception>

#include <libcutl/export.hxx>

namespace cutl
{
  struct LIBCUTL_EXPORT exception: std::exception
  {
    // By default return the exception type name ( typeid (*this).name () ).
    //
    virtual char const*
    what () const noexcept;
  };
}

#endif // LIBCUTL_EXCEPTION_HXX
