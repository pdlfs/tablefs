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
#include "tablefs.h"

#include <sys/stat.h>

namespace pdlfs {

Status Filesystem::Fstat(  ///
    const User& who, const Stat* at, const char* const pathname,
    Stat* const stat) {
  bool has_tailing_slashes(false);
  if (!at) at = &r_.rootstat;
  Stat parent_dir;
  Slice tgt;
  Status status =
      Resolu(who, *at, pathname, &parent_dir, &tgt, &has_tailing_slashes);
  if (!status.ok()) {
    return status;
  }

  if (!tgt.empty()) {
    const uint32_t mode =
        has_tailing_slashes ? S_IFDIR : 0;  // Target must be a dir
    status = Lookup(who, parent_dir, tgt, mode, stat);
  } else {  // Special case in which path is a root
    *stat = r_.rootstat;
  }

  return status;
}

Status Filesystem::Opendir(  ///
    const User& who, const Stat* at, const char* const pathname,
    FilesystemDir** dir) {
  bool has_tailing_slashes(false);
  if (!at) at = &r_.rootstat;
  Stat parent_dir;
  Slice dirname;
  Status status =
      Resolu(who, *at, pathname, &parent_dir, &dirname, &has_tailing_slashes);
  if (!status.ok()) {
    return status;
  }

  status = Getdir(who, parent_dir, dirname, dir);

  return status;
}

Status Filesystem::Readdir(  ///
    FilesystemDir* dir, Stat* stat, std::string* name) {
  MDB::Dir* d = reinterpret_cast<MDB::Dir*>(dir);
  return mdb_->Readdir(d, stat, name);
}

Status Filesystem::Closdir(FilesystemDir* dir) {
  MDB::Dir* d = reinterpret_cast<MDB::Dir*>(dir);
  mdb_->Closedir(d);
  return Status::OK();
}

Status Filesystem::Mkdir(  ///
    const User& who, const Stat* at, const char* const pathname,
    uint32_t mode) {
  bool has_tailing_slashes(false);
  if (!at) at = &r_.rootstat;
  Stat parent_dir;
  Slice dirname;
  Status status =
      Resolu(who, *at, pathname, &parent_dir, &dirname, &has_tailing_slashes);
  if (!status.ok()) {
    return status;
  } else if (dirname.empty()) {  // Special case in which path is a root
    return Status::AlreadyExists(Slice());
  }

  mode = S_IFDIR | (ALLPERMS & mode);
  Stat stat;
  status = Insert(who, parent_dir, dirname, mode, &stat);

  return status;
}

Status Filesystem::Creat(  ///
    const User& who, const Stat* at, const char* const pathname,
    uint32_t mode) {
  bool has_tailing_slashes(false);
  if (!at) at = &r_.rootstat;
  Stat parent_dir;
  Slice fname;
  Status status =
      Resolu(who, *at, pathname, &parent_dir, &fname, &has_tailing_slashes);
  if (!status.ok()) {
    return status;
  } else if (fname.empty()) {  // Special case in which path is a root
    return Status::AlreadyExists(Slice());
  } else if (has_tailing_slashes) {  // Path is a dir
    return Status::FileExpected(Slice());
  }

  mode = S_IFREG | (ALLPERMS & mode);
  Stat stat;
  status = Insert(who, parent_dir, fname, mode, &stat);

  return status;
}

Status Filesystem::Resolu(  ///
    const User& who, const Stat& at, const char* pathname, Stat* parent_dir,
    Slice* last_component, bool* has_tailing_slashes) {
#define PATH_SGMT(pathname, remaining_path) \
  Slice(pathname, remaining_path - pathname)
  const char* remaining_path(NULL);
  Status status =
      Resolv(who, at, pathname, parent_dir, last_component, &remaining_path);
  if (status.IsDirExpected() && remaining_path) {
    return Status::DirExpected(PATH_SGMT(pathname, remaining_path));
  } else if (status.IsNotFound() && remaining_path) {
    return Status::NotFound(PATH_SGMT(pathname, remaining_path));
  } else if (!status.ok()) {
    return status;
  }

  const char* const p = last_component->data();
  if (p[last_component->size()] == '/')  ///
    *has_tailing_slashes = true;

  return status;
}

namespace {

inline Status UnexpectedMode(uint32_t mode) {
  if (mode == S_IFDIR)  ///
    return Status::DirExpected(Slice());
  if (mode == S_IFREG)  ///
    return Status::FileExpected(Slice());
  return Status::AssertionFailed("Unexpected file type");
}

// Return the owner of a given directory.
inline uint32_t uid(const Stat& dir) {  ///
  return dir.UserId();
}

// Return the group owner of a given directory.
inline uint32_t gid(const Stat& dir) {  ///
  return dir.GroupId();
}

bool IsDirReadOk(const FilesystemOptions& options, const Stat& dir,
                 const User& who) {
  const uint32_t mode = dir.FileMode();
  if (options.skip_perm_checks) {
    return true;
  } else if (who.uid == 0) {
    return true;
  } else if (who.uid == uid(dir) && (mode & S_IRUSR) == S_IRUSR) {
    return true;
  } else if (who.gid == gid(dir) && (mode & S_IRGRP) == S_IRGRP) {
    return true;
  } else if ((mode & S_IROTH) == S_IROTH) {
    return true;
  } else {
    return false;
  }
}

bool IsDirWriteOk(const FilesystemOptions& options, const Stat& dir,
                  const User& who) {
  const uint32_t mode = dir.FileMode();
  if (options.skip_perm_checks) {
    return true;
  } else if (who.uid == 0) {
    return true;
  } else if (who.uid == uid(dir) && (mode & S_IWUSR) == S_IWUSR) {
    return true;
  } else if (who.gid == gid(dir) && (mode & S_IWGRP) == S_IWGRP) {
    return true;
  } else if ((mode & S_IWOTH) == S_IWOTH) {
    return true;
  } else {
    return false;
  }
}

bool IsLookupOk(const FilesystemOptions& options, const Stat& dir,
                const User& who) {
  const uint32_t mode = dir.FileMode();
  if (options.skip_perm_checks) {
    return true;
  } else if (who.uid == 0) {
    return true;
  } else if (who.uid == uid(dir) && (mode & S_IXUSR) == S_IXUSR) {
    return true;
  } else if (who.gid == gid(dir) && (mode & S_IXGRP) == S_IXGRP) {
    return true;
  } else if ((mode & S_IXOTH) == S_IXOTH) {
    return true;
  } else {
    return false;
  }
}
}  // namespace

Status Filesystem::Resolv(  ///
    const User& who, const Stat& relative_root, const char* const pathname,
    Stat* parent_dir, Slice* last_component,  ///
    const char** remaining_path) {
  assert(pathname);
  const char* p = pathname;
  const char* q;
  assert(p[0] == '/');
  Stat tmp;
  Status status;
  const Stat* current_parent = &relative_root;
  Slice current_name;
  while (true) {
    // Jump forward to the next path splitter.
    // E.g., "/", "/a/b", "/aa/bb/cc/dd".
    //        ||     | |         |  |
    //        pq     p q         p  q
    for (q = p + 1; q[0]; q++) {
      if (q[0] == '/') {
        break;
      }
    }
    if (!q[0]) {  // End of path
      break;
    }
    // This skips empty names in the beginning of a path.
    // E.g., "///", "//a", "/////a/b/c".
    //         ||    ||        ||
    //         pq    pq        pq
    if (q - p - 1 == 0) {
      p = q;  // I.e., p++
      continue;
    }
    // Look ahead and skip repeated slashes. E.g., "//a//b", "/a/bb////cc".
    //                                               | | |      |  |   |
    //                                               p q c      p  q   c
    // This also gets rid of potential tailing slashes.
    // E.g., "/a/b/", "/a/b/c/////".
    //          | ||       | |    |
    //          p qc       p q    c
    const char* c = q + 1;
    for (; c[0]; c++) {
      if (c[0] != '/') {
        break;
      }
    }
    if (!c[0]) {  // End of path
      break;
    }
    current_name = Slice(p + 1, q - p - 1);
    p = c - 1;
    status = Lookup(who, *current_parent, current_name, S_IFDIR, &tmp);
    if (status.ok()) {
      current_parent = &tmp;
    } else {
      break;
    }
  }
  if (status.ok()) {
    *last_component = Slice(p + 1, q - p - 1);
  } else {
    *last_component = current_name;
    *remaining_path = p;
  }

  *parent_dir = *current_parent;
  return status;
}

Status Filesystem::Lookup(  ///
    const User& who, const Stat& parent_dir, const Slice& name, uint32_t mode,
    Stat* const stat) {
  if (!IsLookupOk(options_, parent_dir, who)) {
    return Status::AccessDenied(Slice());
  }
  Status status = mdb_->Get(DirId(parent_dir), name, stat);
  if (!status.ok()) {
    return status;
  } else if ((stat->FileMode() & mode) != mode) {
    return UnexpectedMode(mode);
  } else {
    return status;
  }
}

Status Filesystem::Insert(  ///
    const User& who, const Stat& parent_dir, const Slice& name, uint32_t mode,
    Stat* const stat) {
  if (!IsDirWriteOk(options_, parent_dir, who)) {
    return Status::AccessDenied(Slice());
  }
  Status status;
  const DirId pdir(parent_dir);
  if (!options_.skip_name_collision_checks) {
    status = mdb_->Get(pdir, name, stat);
    if (status.ok()) {
      return Status::AlreadyExists(Slice());
    } else if (!status.IsNotFound()) {
      return status;
    }
  }

  stat->SetInodeNo(r_.inoseq++);
  stat->SetFileSize(0);
  stat->SetFileMode(mode);
  stat->SetUserId(who.uid);
  stat->SetGroupId(who.gid);
  stat->SetModifyTime(0);
  stat->SetChangeTime(0);
  stat->AssertAllSet();

  status = mdb_->Set(pdir, name, *stat);

  return status;
}

Status Filesystem::Getdir(  ///
    const User& who, const Stat& parent_dir, const Slice& name,
    FilesystemDir** dir) {
  if (!IsLookupOk(options_, parent_dir, who)) {
    return Status::AccessDenied(Slice());
  }
  Stat tmp;
  const DirId pdir(parent_dir);
  const Stat* stat = &r_.rootstat;
  Status status;
  if (!name.empty()) {  // No need to get if target is root
    status = mdb_->Get(pdir, name, &tmp);
    if (!status.ok()) {
      return status;
    } else if (!S_ISDIR(stat->FileMode())) {  // Must be a dir
      return Status::DirExpected(Slice());
    }
    stat = &tmp;
  }
  if (!IsDirReadOk(options_, *stat, who)) {
    return Status::AccessDenied(Slice());
  } else {
    *dir = reinterpret_cast<FilesystemDir*>(mdb_->Opendir(DirId(*stat)));
    return status;
  }
}

Slice FilesystemRoot::EncodeTo(char* scratch) const {
  Slice en = rootstat.EncodeTo(scratch);
  char* p = EncodeVarint64(scratch + en.size(), inoseq);
  return Slice(scratch, p - scratch);
}

bool FilesystemRoot::DecodeFrom(const Slice& encoding) {
  Slice input = encoding;
  return DecodeFrom(&input);
}

bool FilesystemRoot::DecodeFrom(Slice* input) {
  if (!rootstat.DecodeFrom(input))  ///
    return false;
  return GetVarint64(input, &inoseq);
}

FilesystemOptions::FilesystemOptions()
    : skip_name_collision_checks(false), skip_perm_checks(false) {}

Filesystem::Filesystem(const FilesystemOptions& options)
    : options_(options), db_(NULL), mdb_(NULL) {}

namespace {
void InitializeFilesystem(Stat* const root) {
  root->SetInodeNo(0);
  root->SetFileSize(0);
  root->SetFileMode(ACCESSPERMS | S_ISVTX);
  root->SetUserId(0);
  root->SetGroupId(0);
  root->SetModifyTime(0);
  root->SetChangeTime(0);
  root->AssertAllSet();
}
}  // namespace

Status Filesystem::OpenFilesystem(const std::string& fsloc) {
  MDB::DbOpts dbopts;
  dbopts.create_if_missing = true;
  std::string tmp;
  Status status = MDB::Db::Open(dbopts, fsloc, &db_);
  if (!status.ok()) {
    return status;
  }
  mdb_ = new MDB(MDBOptions(db_));
  status = mdb_->LoadFsroot(&tmp);
  if (status.IsNotFound()) {  // This is a new fs image
    InitializeFilesystem(&r_.rootstat);
    r_.inoseq = 2;
    status = Status::OK();
  } else if (status.ok()) {
    if (!r_.DecodeFrom(tmp)) {
      status = Status::Corruption("Cannot recover fs root");
    }
  }
  return status;
}

Filesystem::~Filesystem() {
  char tmp[200];
  if (mdb_) {
    Slice encoding = r_.EncodeTo(tmp);
    mdb_->SaveFsroot(encoding);
    delete mdb_;
  }
  if (db_) {
    delete db_;
  }
}

}  // namespace pdlfs
