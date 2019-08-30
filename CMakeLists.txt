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

#
# configuration/build:
#   - choose a build directory and "cd" to it
#   - cmake [flags] directory
#   - make
#
#  where directory is the top-level source directory where this file resides.
#
#  general cmake flags:
#    -DCMAKE_INSTALL_PREFIX=/usr/local     -- the prefix for installing
#    -DCMAKE_BUILD_TYPE=type               -- type can be DEBUG, RELEASE, ...
#    -DCMAKE_PREFIX_PATH=/dir              -- external packages
#    -DBUILD_SHARED_LIBS=OFF               -- turn ON for shared libs
#    -DBUILD_TESTS=OFF                     -- turn ON to build tests
#
#     note that CMAKE_PREFIX_PATH can be a list of directories:
#      -DCMAKE_PREFIX_PATH='/dir1;/dir2;/dir3'
#
# general PDLFS config compile time options flags:
#   -DPDLFS_GFLAGS=ON                      -- use gflags for arg parsing
#     - GFLAGS_INCLUDE_DIR: optional hint for finding gflags/gflags.h
#     - GFLAGS_LIBRARY_DIR: optional hint for finding gflags lib
#   -DPDLFS_GLOG=ON                        -- use glog for logging
#   -DPDLFS_SNAPPY=ON                      -- compile in snappy compression
#     - SNAPPY_INCLUDE_DIR: optional hint for finding snappy.h
#     - SNAPPY_LIBRARY_DIR: optional hint for finding snappy lib
#   -DPDLFS_VERBOSE=1                      -- set max log verbose level
#
# TABLEFS specific compile time options flags:
#   -DTABLEFS_CXX_STANDARD=11              -- CXX stardard to request
#   -DTABLEFS_CXX_STANDARD_REQUIRED=OFF    -- if CXX stardard must be met
#   -DTABLEFS_COMMON_INTREE=OFF            -- in-tree common lib (for devel)
#
# note: package config files for external packages must be preinstalled in
#       CMAKE_INSTALL_PATH or on CMAKE_PREFIX_PATH, except as noted.
#

#
# note: cmake 2.8.12 is considered stale, and will be deprecated.
# yet cmake 2.8.12.2 is shipped by ubuntu14.04. ubuntu14.04 won't be end of
# life until Apr 2019, though cmake 3 was later backported to ubuntu14.04
# as cmake3 (use ``sudo apt-get install cmake3'' to install).
cmake_minimum_required (VERSION 2.8.12)

project (TABLEFS)

# add pdlfs-common cmake module directory to the path
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
        "${CMAKE_CURRENT_SOURCE_DIR}/external/pdlfs-common/cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# disable a subset of pdlfs modules that we don't need
set (PDLFS_DFS_COMMON  "OFF" CACHE BOOL "Include common DFS code")
set (PDLFS_SILT_ECT    "OFF" CACHE BOOL "Include SILT ECT code")
set (PDLFS_MARGO_RPC   "OFF" CACHE BOOL "Use Margo RPC")
set (PDLFS_MERCURY_RPC "OFF" CACHE BOOL "Use Mercury RPC")
set (PDLFS_RADOS       "OFF" CACHE BOOL "Use RADOS OSD")
mark_as_advanced(PDLFS_DFS_COMMON PDLFS_SILT_ECT PDLFS_MARGO_RPC
                 PDLFS_MERCURY_RPC PDLFS_RADOS)

#
# we compile everything with -DTABLEFS by attaching it as a property of
# the common lib.  we also set the common library's name to tablefs-common
# (since we may add tablefs-specific code to it)  we request (but don't
# require) C++ 11 standard for possible performance improvements due it
# its move semantics.
#
set (PDLFS_COMMON_LIBNAME "tablefs-common" CACHE
     STRING "Custom name to install tablefs-common with")
set (PDLFS_COMMON_DEFINES "TABLEFS" CACHE
     STRING "Additional defines for this version of pdlfs-common")
mark_as_advanced (PDLFS_COMMON_LIBNAME PDLFS_COMMON_DEFINES)
set (TABLEFS_CXX_STANDARD "11" CACHE STRING "C++ std to probe")
set (TABLEFS_CXX_STANDARD_REQUIRED "OFF" CACHE BOOL "C++ std must be met")
mark_as_advanced (TABLEFS_CXX_STANDARD TABLEFS_CXX_STANDARD_REQUIRED)
set_property (CACHE TABLEFS_CXX_STANDARD PROPERTY STRINGS "98" "11")

# note: CMAKE_CXX_STANDARD is not defined until cmake 3.1, and will be
# ignored by cmake 3.0 and before.
#
include (CheckCXXCompilerFlag)
if (CMAKE_VERSION VERSION_LESS "3.1")
    set (cxxstdflag "-std=c++${TABLEFS_CXX_STANDARD}")
    CHECK_CXX_COMPILER_FLAG (${cxxstdflag} flag${cxxstdflag})
    if (${flag${cxxstdflag}})
        add_compile_options (${cxxstdflag})
    else ()
        if (TABLEFS_CXX_STANDARD_REQUIRED)
            message (FATAL_ERROR "Fail to enable CXX ${TABLEFS_CXX_STANDARD}")
        endif ()
    endif ()
else ()
    set (CMAKE_CXX_STANDARD ${TABLEFS_CXX_STANDARD})
    set (CMAKE_CXX_STANDARD_REQUIRED ${TABLEFS_CXX_STANDARD_REQUIRED})
endif ()

# pull in pdlfs handling of generic cmake config
include (cmake-options)

# handle all the common PDLFS options using cmake/pdlfs-options.cmake
include (pdlfs-options)

# port tablefs to a different db backend
set (TABLEFS_PORT_LEVELDB "OFF" CACHE BOOL "Port to LEVELDB")
set (TABLEFS_PORT_KVRANGE "OFF" CACHE BOOL "Port to KVRANGE")

# user hooks to configure tablefs
set (TABLEFS_COMMON_INTREE "OFF" CACHE BOOL
     "Build in-tree common lib (for devel)")

#
# we build the in-tree pdlfs-common if TABLEFS_COMMON_INTREE is set,
# otherwise we look for one already built in our install or prefix path.
#
if (TABLEFS_COMMON_INTREE)
    add_subdirectory (external/pdlfs-common/src)
else ()
    message ("OK ${PDLFS_COMPONENT_CFG}") # XXXCDC
    find_package (tablefs-common REQUIRED COMPONENTS ${PDLFS_COMPONENT_CFG})
endif ()

add_subdirectory (src)

