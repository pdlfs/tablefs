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

#include "tablefs/tablefs_api.h"
#include "tablefs.h"

#include <errno.h>
#ifndef EHOSTUNREACH
#define EHOSTUNREACH ENODEV
#endif

#ifndef ENOSYS
#define ENOSYS EPERM
#endif

#ifndef ENOBUFS
#define ENOBUFS ENOMEM
#endif

namespace {
inline pdlfs::Status BadArgs() {
  return pdlfs::Status::InvalidArgument(pdlfs::Slice());
}

void SetErrno(const pdlfs::Status& s) {
  if (s.ok()) {
    errno = 0;
  } else if (s.IsNotFound()) {
    errno = ENOENT;
  } else if (s.IsAlreadyExists()) {
    errno = EEXIST;
  } else if (s.IsFileExpected()) {
    errno = EISDIR;
  } else if (s.IsDirExpected()) {
    errno = ENOTDIR;
  } else if (s.IsInvalidFileDescriptor()) {
    errno = EBADF;
  } else if (s.IsTooManyOpens()) {
    errno = EMFILE;
  } else if (s.IsAccessDenied()) {
    errno = EACCES;
  } else if (s.IsAssertionFailed()) {
    errno = EPERM;
  } else if (s.IsReadOnly()) {
    errno = EROFS;
  } else if (s.IsNotSupported()) {
    errno = ENOSYS;
  } else if (s.IsInvalidArgument()) {
    errno = EINVAL;
  } else if (s.IsBufferFull()) {
    errno = ENOBUFS;
  } else if (s.IsRange()) {
    errno = ERANGE;
  } else {
    errno = EIO;
  }
}
}  // namespace

extern "C" {
struct tablefs {
  pdlfs::FilesystemOptions* fsopts;
  pdlfs::Filesystem* fs;
  pdlfs::User me;
};

struct tablefs_dir {
  struct dirent buf;
  pdlfs::FilesystemDir* dir;
  tablefs_t* h;
};
}

namespace {
inline void SetTimespec(struct timespec* const ts, uint64_t micros) {
  if (micros >= 1000000) {
    ts->tv_sec = micros / 1000000;
    ts->tv_nsec = (micros - ts->tv_sec * 1000000) * 1000;
  } else {
    ts->tv_sec = 0;
    ts->tv_nsec = micros * 1000;
  }
}
// XXX: h may be NULL
int FilesystemError(tablefs_t* h, const pdlfs::Status& s) {
  SetErrno(s);
  return -1;
}
// XXX: dh may be NULL
int DirError(tablefs_dir_t* dh, const pdlfs::Status& s) {
  SetErrno(s);
  return -1;
}
}  // namespace

extern "C" {

tablefs_t* tablefs_newfshdl() {
  tablefs_t* h = static_cast<tablefs_t*>(malloc(sizeof(struct tablefs)));
  h->fsopts = new pdlfs::FilesystemOptions;
  h->fs = new pdlfs::Filesystem(*h->fsopts);
  h->me.uid = getuid();
  h->me.gid = getgid();
  return h;
}

int tablefs_openfs(tablefs_t* h, const char* fsloc) {
  pdlfs::Status status;
  if (!h) {
    status = BadArgs();
  } else {
    status = h->fs->OpenFilesystem(fsloc);
  }

  if (!status.ok()) {
    return FilesystemError(h, status);
  } else {
    return 0;
  }
}

int tablefs_closefs(tablefs_t* h) {
  if (h) {
    delete h->fsopts;
    delete h->fs;
    free(h);
  }
}

int tablefs_mkfil(tablefs_t* h, const char* path, uint32_t mode) {
  pdlfs::Status status;
  if (!h) {
    status = BadArgs();
  } else if (!path || path[0] != '/') {
    status = BadArgs();
  } else {
    status = h->fs->Mkfil(h->me, NULL, path, mode);
  }

  if (!status.ok()) {
    return FilesystemError(h, status);
  } else {
    return 0;
  }
}

int tablefs_mkdir(tablefs_t* h, const char* path, uint32_t mode) {
  pdlfs::Status status;
  if (!h) {
    status = BadArgs();
  } else if (!path || path[0] != '/') {
    status = BadArgs();
  } else {
    status = h->fs->Mkdir(h->me, NULL, path, mode);
  }

  if (!status.ok()) {
    return FilesystemError(h, status);
  } else {
    return 0;
  }
}

int tablefs_lstat(tablefs_t* h, const char* path, struct stat* const buf) {
  pdlfs::Status status;
  pdlfs::Stat stat;
  if (!h) {
    status = BadArgs();
  } else if (!path || path[0] != '/') {
    status = BadArgs();
  } else if (!buf) {
    status = BadArgs();
  } else {
    status = h->fs->Lstat(h->me, NULL, path, &stat);  /// XXX: NO atime
    if (status.ok()) {
      SetTimespec(&buf->st_atimespec, stat.ModifyTime());
      SetTimespec(&buf->st_mtimespec, stat.ModifyTime());
      SetTimespec(&buf->st_ctimespec, stat.ChangeTime());
      buf->st_ino = stat.InodeNo();
      buf->st_size = stat.FileSize();
      buf->st_mode = stat.FileMode();
      buf->st_uid = stat.UserId();
      buf->st_gid = stat.GroupId();
      buf->st_nlink = 1;
    }
  }

  if (!status.ok()) {
    return FilesystemError(h, status);
  } else {
    return 0;
  }
}

tablefs_dir_t* tablefs_opendir(tablefs_t* h, const char* path) {
  pdlfs::FilesystemDir* dir;
  pdlfs::Status status;
  if (!h) {
    status = BadArgs();
  } else if (!path || path[0] != '/') {
    status = BadArgs();
  } else {
    status = h->fs->Opendir(h->me, NULL, path, &dir);
  }

  if (!status.ok()) {
    FilesystemError(h, status);
    return NULL;
  } else {
    tablefs_dir_t* const dh =
        static_cast<tablefs_dir_t*>(malloc(sizeof(struct tablefs_dir)));
    dh->dir = dir;
    dh->h = h;
    return dh;
  }
}

struct dirent* tablefs_readdir(tablefs_dir_t* dh) {
  pdlfs::Status status;
  pdlfs::Stat stat;
  std::string entname;
  if (!dh) {
    status = BadArgs();
  } else {
    status = dh->h->fs->Readdir(dh->dir, &stat, &entname);
  }

  if (!status.ok()) {
    if (!status.IsNotFound())  /// POSIX says EOD is not an error.
      DirError(dh, status);
    return NULL;
  } else {
    struct dirent* const buf = &dh->buf;
    strcpy(buf->d_name, entname.c_str());
    buf->d_type = IFTODT(stat.FileMode());
    buf->d_ino = stat.InodeNo();
    return buf;
  }
}

int tablefs_closedir(tablefs_dir_t* dh) {
  pdlfs::Status status;
  if (!dh) {
    status = BadArgs();
  } else {
    status = dh->h->fs->Closdir(dh->dir);
    if (status.ok()) {
      free(dh);
    }
  }

  if (!status.ok()) {
    return DirError(dh, status);
  } else {
    return 0;
  }
}

}  // extern "C"
