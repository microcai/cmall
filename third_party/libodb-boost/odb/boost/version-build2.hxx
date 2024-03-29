// file      : odb/boost/version-build2.hxx.in
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef LIBODB_BOOST_VERSION // Note: using the version macro itself.

// @@ TODO: need to derive automatically (it is also hardcoded in *.options).
//
#define ODB_BOOST_VERSION     2047300

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
#define LIBODB_BOOST_VERSION       200004999995230ULL
#define LIBODB_BOOST_VERSION_STR   "2.5.0-b.23"
#define LIBODB_BOOST_VERSION_ID    "2.5.0-b.23"

#define LIBODB_BOOST_VERSION_MAJOR 2
#define LIBODB_BOOST_VERSION_MINOR 5
#define LIBODB_BOOST_VERSION_PATCH 0

#define LIBODB_BOOST_PRE_RELEASE   true

#define LIBODB_BOOST_SNAPSHOT      0ULL
#define LIBODB_BOOST_SNAPSHOT_ID   ""

#include <odb/version.hxx>

#ifdef LIBODB_VERSION
#  if !(LIBODB_VERSION == 200004999995230ULL)
#    error incompatible libodb version, libodb == 2.5.0-b.23 is required
#  endif
#endif

#endif // LIBODB_BOOST_VERSION
