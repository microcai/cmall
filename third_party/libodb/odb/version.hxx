// file      : odb/version.hxx
// license   : GNU GPL v2; see accompanying LICENSE file

#ifdef LIBODB_BUILD2
#  include <odb/version-build2.hxx>
#else

#ifndef ODB_VERSION_HXX
#define ODB_VERSION_HXX

#include <odb/pre.hxx>

// Version format is AABBCCDD where
//
// AA - major version number
// BB - minor version number
// CC - bugfix version number
// DD - alpha / beta (DD + 50) version number
//
// When DD is not 00, 1 is subtracted from AABBCC. For example:
//
// Version     AABBCCDD
// 2.0.0       02000000
// 2.1.0       02010000
// 2.1.1       02010100
// 2.2.0.a1    02019901
// 3.0.0.b2    02999952
//

// ODB interface version: minor, major, and alpha/beta versions.
//
#define ODB_VERSION     20473
#define ODB_VERSION_STR "2.5-b.23"

// libodb version: interface version plus the bugfix version.
//
#define LIBODB_VERSION     2049973
#define LIBODB_VERSION_STR "2.5.0-b.23"

#include <odb/post.hxx>

#endif // ODB_VERSION_HXX
#endif // LIBODB_BUILD2
