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
  pdlfs::FilesystemOptions options;
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
    status = h->fs->Lstat(h->me, NULL, path, &stat);
    if (status.ok()) {
      memset(buf, 0, sizeof(struct stat));
      buf->st_ino = stat.InodeNo();
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
    memset(&dh->buf, 0, sizeof(struct dirent));
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
    buf->d_ino = stat.InodeNo();
    buf->d_type =  ///
        S_ISDIR(stat.FileMode()) ? DT_DIR : DT_REG;
    strcpy(buf->d_name, entname.c_str());
    buf->d_namlen = entname.size();
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
