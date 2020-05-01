/*
 * Copyright (c) 2019 Carnegie Mellon University,
 * Copyright (c) 2019 Triad National Security, LLC, as operator of
 *     Los Alamos National Laboratory.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * with the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of CMU, TRIAD, Los Alamos National Laboratory, LANL, the
 *    U.S. Government, nor the names of its contributors may be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
int tablefs_set_readonly(tablefs_t* h, int flg);
/* Open a filesystem image at a given location */
int tablefs_openfs(tablefs_t* h, const char* fsloc);
/* Close a filesystem image and delete its handle */
int tablefs_closefs(tablefs_t* h);
/* Retrieve file status */
int tablefs_lstat(tablefs_t* h, const char* path, struct stat* stat);
/* Create a regular file at a specified path */
int tablefs_mkfile(tablefs_t* h, const char* path, uint32_t mode);
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
