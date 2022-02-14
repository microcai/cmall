// file      : cutl/details/config.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef CUTL_DETAILS_CONFIG_HXX
#define CUTL_DETAILS_CONFIG_HXX

// C++11 support.
//
#ifdef _MSC_VER
#  if _MSC_VER >= 1900
#    define LIBCUTL_CXX11
#  endif
#else
#  if defined(__GXX_EXPERIMENTAL_CXX0X__) || __cplusplus >= 201103L
#    ifdef __clang__ // Pretends to be a really old __GNUC__ on some platforms.
#      define LIBCUTL_CXX11
#    elif defined(__GNUC__)
#      if (__GNUC__ == 4 && __GNUC_MINOR__ >= 9) || __GNUC__ > 4
#        define LIBCUTL_CXX11
#      endif
#    else
#      define LIBCUTL_CXX11
#    endif
#  endif
#endif

#ifdef LIBCUTL_CXX11
#  define LIBCUTL_NOTHROW_NOEXCEPT noexcept
#else
#  define LIBCUTL_NOTHROW_NOEXCEPT throw()
#endif

#ifdef LIBCUTL_BUILD2
#  ifndef LIBCUTL_CXX11
#    error C++ compiler does not support (enough of) C++11
#  endif
#  ifdef _MSC_VER
#    include <cutl/details/build2/config-vc.h>
#  else
#    include <cutl/details/build2/config.h>
#  endif
#else
#  ifdef _MSC_VER
#    include <cutl/details/config-vc.h>
#  else
#    include <cutl/details/config.h>
#  endif
#endif

#endif // CUTL_DETAILS_CONFIG_HXX
