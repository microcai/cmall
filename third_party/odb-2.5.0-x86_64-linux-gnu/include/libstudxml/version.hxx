// file      : libstudxml/version.hxx.in -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifndef LIBSTUDXML_VERSION // Note: using the version macro itself.

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
#define LIBSTUDXML_VERSION       100000999995100ULL
#define LIBSTUDXML_VERSION_STR   "1.1.0-b.10"
#define LIBSTUDXML_VERSION_ID    "1.1.0-b.10"

#define LIBSTUDXML_VERSION_MAJOR 1
#define LIBSTUDXML_VERSION_MINOR 1
#define LIBSTUDXML_VERSION_PATCH 0

#define LIBSTUDXML_PRE_RELEASE   true

#define LIBSTUDXML_SNAPSHOT      0ULL
#define LIBSTUDXML_SNAPSHOT_ID   ""

#endif // LIBSTUDXML_VERSION
