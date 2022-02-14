// file      : cutl/details/export.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef CUTL_DETAILS_EXPORT_HXX
#define CUTL_DETAILS_EXPORT_HXX

#include <cutl/details/config.hxx>

// Normally we don't export class templates (but do complete specializations),
// inline functions, and classes with only inline member functions. Exporting
// classes that inherit from non-exported/imported bases (e.g., std::string)
// will end up badly. The only known workarounds are to not inherit or to not
// export. Also, MinGW GCC doesn't like seeing non-exported function being
// used before their inline definition. The workaround is to reorder code. In
// the end it's all trial and error.

#ifdef LIBCUTL_BUILD2

#if defined(LIBCUTL_STATIC)         // Using static.
#  define LIBCUTL_EXPORT
#elif defined(LIBCUTL_STATIC_BUILD) // Building static.
#  define LIBCUTL_EXPORT
#elif defined(LIBCUTL_SHARED)       // Using shared.
#  ifdef _WIN32
#    define LIBCUTL_EXPORT __declspec(dllimport)
#  else
#    define LIBCUTL_EXPORT
#  endif
#elif defined(LIBCUTL_SHARED_BUILD) // Building shared.
#  ifdef _WIN32
#    define LIBCUTL_EXPORT __declspec(dllexport)
#  else
#    define LIBCUTL_EXPORT
#  endif
#else
// If none of the above macros are defined, then we assume we are being used
// by some third-party build system that cannot/doesn't signal the library
// type. Note that this fallback works for both static and shared but in case
// of shared will be sub-optimal compared to having dllimport.
//
#  define LIBCUTL_EXPORT            // Using static or shared.
#endif

#else // LIBCUTL_BUILD2

#ifdef LIBCUTL_STATIC_LIB
#  define LIBCUTL_EXPORT
#else
#  ifdef _WIN32
#    ifdef _MSC_VER
#      ifdef LIBCUTL_DYNAMIC_LIB
#        define LIBCUTL_EXPORT __declspec(dllexport)
#      else
#        define LIBCUTL_EXPORT __declspec(dllimport)
#      endif
#    else
#      ifdef LIBCUTL_DYNAMIC_LIB
#        ifdef DLL_EXPORT
#          define LIBCUTL_EXPORT __declspec(dllexport)
#        else
#          define LIBCUTL_EXPORT
#        endif
#      else
#        define LIBCUTL_EXPORT __declspec(dllimport)
#      endif
#    endif
#  else
#    define LIBCUTL_EXPORT
#  endif
#endif

#endif // LIBCUTL_BUILD2

#endif // CUTL_DETAILS_EXPORT_HXX
