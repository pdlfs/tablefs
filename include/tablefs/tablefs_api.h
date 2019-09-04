/*
 * Copyright (c) 2019 Carnegie Mellon University,
 * Copyright (c) 2019 Triad National Security, LLC, as operator of
 *     Los Alamos National Laboratory.
 *
 * All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. See the AUTHORS file for names of contributors.
 */
#pragma once

#include <dirent.h>
#include <stdint.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

struct tablefs; /* Opaque handle to a filesystem instance */
typedef struct tablefs tablefs_t;
tablefs_t* tablefs_newfshdl();
/* Open a filesystem image at a given location */
int tablefs_openfs(tablefs_t* h, const char* fsloc);
int tablefs_closefs(tablefs_t* h);
/* Retrieve file status */
int tablefs_lstat(tablefs_t* h, const char* path, struct stat* stat);
/* Create a regular file at a specified path */
int tablefs_mkfil(tablefs_t* h, const char* path, uint32_t mode);
/* Create a filesystem directory at a specified path */
int tablefs_mkdir(tablefs_t* h, const char* path, uint32_t mode);
struct tablefs_dir; /* Opaque handle to an opened filesystem directory */
typedef struct tablefs_dir tablefs_dir_t;
tablefs_dir_t* tablefs_opendir(tablefs_t* h, const char* path);
struct dirent* tablefs_readdir(tablefs_dir_t* dh);
int tablefs_closedir(tablefs_dir_t* dh);

#ifdef __cplusplus
}
#endif
