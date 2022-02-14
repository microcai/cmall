// file      : libbpkg/version.hxx.in -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBPKG_VERSION // Note: using the version macro itself.

// The numeric version format is AAAAABBBBBCCCCCDDDE where:
//
// AAAAA - major version number
// BBBBB - minor version number
// CCCCC - bugfix version number
// DDD   - alpha / beta (DDD + 500) version number
// E     - final (0) / snapshot (1)
//
// When DDDE is not 0, 1 is subtracted from AAAAABBBBBCCCCC. For example:
//
// Version      AAAAABBBBBCCCCCDDDE
//
// 0.1.0        0000000001000000000
// 0.1.2        0000000001000020000
// 1.2.3        0000100002000030000
// 2.2.0-a.1    0000200001999990010
// 3.0.0-b.2    0000299999999995020
// 2.2.0-a.1.z  0000200001999990011
//
#define LIBBPKG_VERSION       13000000000ULL
#define LIBBPKG_VERSION_STR   "0.13.0"
#define LIBBPKG_VERSION_ID    "0.13.0"

#define LIBBPKG_VERSION_MAJOR 0
#define LIBBPKG_VERSION_MINOR 13
#define LIBBPKG_VERSION_PATCH 0

#define LIBBPKG_PRE_RELEASE   false

#define LIBBPKG_SNAPSHOT      0ULL
#define LIBBPKG_SNAPSHOT_ID   ""

#include <libbutl/version.hxx>

#ifdef LIBBUTL_VERSION
#  if !(LIBBUTL_VERSION >= 13000000000ULL && LIBBUTL_VERSION < 13999990001ULL)
#    error incompatible libbutl version, libbutl ^0.13.0 is required
#  endif
#endif

#endif // LIBBPKG_VERSION
