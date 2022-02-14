// file      : libbuild2/export.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

// Normally we don't export class templates (but do complete specializations),
// inline functions, and classes with only inline member functions. Exporting
// classes that inherit from non-exported/imported bases (e.g., std::string)
// will end up badly. The only known workarounds are to not inherit or to not
// export. Also, MinGW GCC doesn't like seeing non-exported functions being
// used before their inline definition. The workaround is to reorder code. In
// the end it's all trial and error.
//
// Exportation of explicit template instantiations is even hairier: MinGW GCC
// requires __declspec(dllexport) on the extern template declaration while VC
// wants it on the definition. Use LIBBUILD2_{DEC,DEF}EXPORT for that.
//

#if defined(LIBBUILD2_STATIC)         // Using static.
#  define LIBBUILD2_SYMEXPORT
#  define LIBBUILD2_DECEXPORT
#elif defined(LIBBUILD2_STATIC_BUILD) // Building static.
#  define LIBBUILD2_SYMEXPORT
#  define LIBBUILD2_DECEXPORT
#  define LIBBUILD2_DEFEXPORT
#elif defined(LIBBUILD2_SHARED)       // Using shared.
#  ifdef _WIN32
#    define LIBBUILD2_SYMEXPORT __declspec(dllimport)
#    define LIBBUILD2_DECEXPORT __declspec(dllimport)
#  else
#    define LIBBUILD2_SYMEXPORT
#    define LIBBUILD2_DECEXPORT
#  endif
#elif defined(LIBBUILD2_SHARED_BUILD) // Building shared.
#  ifdef _WIN32
#    define LIBBUILD2_SYMEXPORT __declspec(dllexport)
#    if defined(_MSC_VER)
#      define LIBBUILD2_DECEXPORT
#      define LIBBUILD2_DEFEXPORT __declspec(dllexport)
#    else
#      define LIBBUILD2_DECEXPORT __declspec(dllexport)
#      define LIBBUILD2_DEFEXPORT
#    endif
#  else
#    define LIBBUILD2_SYMEXPORT
#    define LIBBUILD2_DECEXPORT
#    define LIBBUILD2_DEFEXPORT
#  endif
#else
// If none of the above macros are defined, then we assume we are being used
// by some third-party build system that cannot/doesn't signal the library
// type. Note that this fallback works for both static and shared but in case
// of shared will be sub-optimal compared to having dllimport. Also note that
// bootstrap ends up here as well.
//
// Using static or shared.
//
#  define LIBBUILD2_SYMEXPORT
#  define LIBBUILD2_DECEXPORT
#  define LIBBUILD2_DEFEXPORT
#endif
