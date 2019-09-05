# Copyright (c) 2019 Carnegie Mellon University,
# Copyright (c) 2019 Triad National Security, LLC, as operator of
#     Los Alamos National Laboratory.
# All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# with the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 3. Neither the name of CMU, TRIAD, Los Alamos National Laboratory, LANL, the
#    U.S. Government, nor the names of its contributors may be used to endorse
#    or promote products derived from this software without specific prior
#    written permission.
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
# EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#
# CMakeLists.txt  cmake file for libtablefs directory
# 16-Jun-2016  chuck@ece.cmu.edu
#

#
# this file is either included from ../CMakeLists.txt or some other
# file if we are being embedded within another project.
#

#
# list of source files and tests
#

# main directory sources and tests
set (tablefs-srcs tablefs.cc tablefs_api.cc)
set (tablefs-tests tablefs_test.cc tablefs_api_test.cc)

# configure/load in standard modules we plan to use
include (CMakePackageConfigHelpers)
set (CMAKE_THREAD_PREFER_PTHREAD TRUE)
set (THREADS_PREFER_PTHREAD_FLAG TRUE)

# note: cmake 3.1 and newer define a Threads::Threads imported target
# that we should switch to when we are ready to require 3.1 or better.
# cmake 3.1 was released late 2014.
find_package (Threads REQUIRED)

# common lib is always required
set (TABLEFS_REQUIRED_PACKAGES "tablefs-common")

# where the cmake package files get installed (for find_package)
set (tfs-pkg-loc "share/cmake/tablefs")

#
# library version set here (e.g. for generating shared libs if we want
#
set (TABLEFS_VERSION_MAJOR 1)
set (TABLEFS_VERSION_MINOR 0)
set (TABLEFS_VERSION_PATCH 0)
set (tfs-vers "${TABLEFS_VERSION_MAJOR}.${TABLEFS_VERSION_MINOR}")
set (TABLEFS_VERSION "${tfs-vers}.${TABLEFS_VERSION_PATCH}")

# setup and configure tablefs_config.h.in
configure_file ("../../include/tablefs/tablefs_config.h.in"
                "../../include/tablefs/tablefs_config.h" @ONLY)

if (TABLEFS_PORT_LEVELDB)
    set (tablefs-port-srcs port/port_leveldb.cc)
elseif (TABLEFS_PORT_KVRANGE)
    set (tablefs-port-srcs port/port_kvrange.cc)
else ()
    set (tablefs-port-srcs port/port_default.cc)
endif ()

#
# generate complete list of files and tests from the parts
#
set (tablefs-all-srcs ${tablefs-port-srcs} ${tablefs-srcs})
set (tablefs-all-tests ${tablefs-tests})

#
# create the library target (user can specify shared vs. static
# using BUILD_SHARED_LIBS).   arrange for users of our lib to
# get the correct -Is for our include directories.  (for building
# we need a -I pointing to the build dir for pdlfs_config_expand.h)
#
add_library (tablefs ${tablefs-all-srcs})
target_include_directories (tablefs PUBLIC $<INSTALL_INTERFACE:include>)
target_include_directories (tablefs BEFORE PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../../include>)
target_link_libraries (tablefs tablefs-common)
if (THREADS_HAVE_PTHREAD_ARG)
    target_compile_options(tablefs PUBLIC "-pthread")
endif()
if (CMAKE_THREAD_LIBS_INIT)
    target_link_libraries (tablefs "${CMAKE_THREAD_LIBS_INIT}")
endif ()

if (TABLEFS_PORT_LEVELDB)
    target_compile_definitions(tablefs PUBLIC "TABLEFS_PORT_LEVELDB")
    find_package (Leveldb MODULE REQUIRED)
    target_link_libraries(tablefs leveldb)
elseif (TABLEFS_PORT_KVRANGE)
    target_compile_definitions(tablefs PUBLIC "TABLEFS_PORT_KVRANGE")
    find_package (Kvrange MODULE REQUIRED)
    target_link_libraries(tablefs kvrange)
endif ()

set_target_properties(tablefs PROPERTIES VERSION ${TABLEFS_VERSION}
                      SOVERSION ${TABLEFS_VERSION_MAJOR})

#
#
# here's the compiler/machine/os dependent stuff...
# this likely isn't that useful (carryover from leveldb)
#

# "-pthread" should take care of all thread related options.
# but if we don't have it, fall back to -D_REENTRANT
if (NOT THREADS_HAVE_PTHREAD_ARG)
    target_compile_definitions (tablefs PUBLIC -D_REENTRANT)
endif ()

# XXX: untested solaris, does it need "-mt"?
# XXX: leveldb also added "-lrt" .. but is that really needed?
if (${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
    CHECK_CXX_COMPILER_FLAG (-mt flag-mt)
    if (${flag-mt})
        target_compile_options (tablefs PUBLIC -mt)
    endif ()
endif ()

#
# end of the compiler/machine/os dependent stuff!
#

#
# installation stuff (packaging and install commands)
#
write_basic_package_version_file(
    "tablefs-config-version.cmake"
    VERSION ${TABLEFS_VERSION}
    COMPATIBILITY AnyNewerVersion)

# generate our config file for find_package()
configure_file (tablefs-config.cmake.in tablefs-config.cmake @ONLY)

# "make install" rules...
install (TARGETS tablefs EXPORT tablefs-targets
         ARCHIVE DESTINATION lib
         LIBRARY DESTINATION lib)
install (EXPORT tablefs-targets
         DESTINATION ${tfs-pkg-loc})
install (FILES "${CMAKE_CURRENT_BINARY_DIR}/tablefs-config.cmake"
               "${CMAKE_CURRENT_BINARY_DIR}/tablefs-config-version.cmake"
         DESTINATION ${tfs-pkg-loc})
install (FILES "../../cmake/FindLeveldb.cmake"
               "../../cmake/FindKvrange.cmake"
         DESTINATION ${tfs-pkg-loc})
install (DIRECTORY ../../include/tablefs
         DESTINATION include
         FILES_MATCHING PATTERN "*.h")
install (FILES
   "${CMAKE_CURRENT_BINARY_DIR}/../../include/tablefs/tablefs_config.h"
   DESTINATION include/tablefs)

#
# tests... we EXCLUDE_FROM_ALL the tests and use pdlfs-options.cmake's
# pdl-build-tests target for building.
#
foreach (lcv ${tablefs-all-tests})

    # use basename as test name
    get_filename_component (id ${lcv} NAME_WE)

    add_executable (${id} EXCLUDE_FROM_ALL ${lcv})
    target_link_libraries (${id} tablefs)
    add_test (${id} ${id})
    add_dependencies (pdl-build-tests ${id})

endforeach ()

