// file      : cutl/exception.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef CUTL_EXCEPTION_HXX
#define CUTL_EXCEPTION_HXX

#include <exception>

#include <cutl/details/config.hxx>
#include <cutl/details/export.hxx>

namespace cutl
{
  struct LIBCUTL_EXPORT exception: std::exception
  {
    // By default return the exception type name ( typeid (*this).name () ).
    //
    virtual char const*
    what () const LIBCUTL_NOTHROW_NOEXCEPT;
  };
}

#endif // CUTL_EXCEPTION_HXX
