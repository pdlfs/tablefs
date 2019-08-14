#
# Copyright (c) 2019 Carnegie Mellon University,
# Copyright (c) 2019 Triad National Security, LLC, as operator of
#     Los Alamos National Laboratory.
#
# All rights reserved.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file. See the AUTHORS file for names of contributors.
#

include (CheckCXXCompilerFlag)

#
# set some general tablefs compile options (this is a directory property)
#
set (tablefs-try-common-flags -Wpedantic -Wall)
list (APPEND tablefs-try-common-flags -Wno-long-long -Wno-sign-compare)
foreach (lcv ${tablefs-try-common-flags})
    CHECK_CXX_COMPILER_FLAG (${lcv} flag${lcv})
    if (${flag${lcv}})
        add_compile_options (${lcv})
    endif ()
endforeach ()

add_subdirectory (libtablefs)

