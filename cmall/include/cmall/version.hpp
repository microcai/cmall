//
// Copyright (C) 2013 - 2021 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#pragma once

#ifndef CMALL_VERSION_MAJOR
#define CMALL_VERSION_MAJOR -1
#endif // !CMALL_VERSION_MAJOR

#ifndef CMALL_VERSION_MINOR
#define CMALL_VERSION_MINOR 12
#endif //CMALL_VERSION_MINOR

#ifndef CMALL_VERSION_TINY
#define CMALL_VERSION_TINY 341
#endif //CMALL_VERSION_TINY

// the format of this version is: MMmmtt
// M = Major version, m = minor version, t = tiny version
#define CMALL_VERSION_NUM ((CMALL_VERSION_MAJOR * 10000) + (CMALL_VERSION_MINOR * 100) + CMALL_VERSION_TINY)
#define CMALL_VERSION "-1.12.341"

#ifndef CMALL_GIT_REVISION
#define CMALL_GIT_REVISION "Git-"
#endif

#define CMALL_VERSION_MIME "CMALL/" CMALL_VERSION "(" CMALL_GIT_REVISION ")"
