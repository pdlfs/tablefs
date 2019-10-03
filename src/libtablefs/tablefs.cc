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
#include "tablefs.h"

#include "pdlfs-common/lru.h"

#include <sys/stat.h>

namespace pdlfs {

// Lookup cache for speeding up pathname resolutions.
struct FilesystemLookupCache {
  explicit FilesystemLookupCache(size_t cap) : lru_(cap) {}
  typedef LRUEntry<Stat> Handle;
  LRUCache<Handle> lru_;
};

// Root information of a filesystem image.
struct FilesystemRoot {
  FilesystemRoot() {}  // Intentionally not initialized for performance.
  // Encode state into a buf space.
  Slice EncodeTo(char* scratch) const;
  // Recover state from a given encoding string.
  // Return True on success, False otherwise.
  bool DecodeFrom(const Slice& encoding);
  bool DecodeFrom(Slice* input);

  uint64_t inoseq;
  Stat rootstat;
};

Status Filesystem::Lstat(  ///
    const User& who, const Stat* at, const char* const pathname,
    Stat* const stat) {
  bool has_tailing_slashes(false);
  if (!at) at = &r_->rootstat;
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
    *stat = r_->rootstat;
  }

  return status;
}

Status Filesystem::Opendir(  ///
    const User& who, const Stat* at, const char* const pathname,
    FilesystemDir** dir) {
  bool has_tailing_slashes(false);
  if (!at) at = &r_->rootstat;
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
  if (!at) at = &r_->rootstat;
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

Status Filesystem::Mkfil(  ///
    const User& who, const Stat* at, const char* const pathname,
    uint32_t mode) {
  bool has_tailing_slashes(false);
  if (!at) at = &r_->rootstat;
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
    const User& who, const Stat& at, const char* const pathname,
    Stat* parent_dir, Slice* last_component,  ///
    bool* has_tailing_slashes) {
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
    // If caching is enabled, result may be read (copied) from the cache instead
    // of the filesystem's DB instance. No cache handle or reference counting
    // stuff is exposed to us (the caller) keeping semantics simple
    status = LookupWithCache(cache_, who, *current_parent, current_name,
                             S_IFDIR, &tmp);
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

namespace {
void DeleteStat(  /// Delete stat inside a cache entry
    const Slice& key, Stat* stat) {
  delete stat;
}
}  // namespace

Status Filesystem::LookupWithCache(  ///
    FilesystemLookupCache* const c, const User& who, const Stat& parent_dir,
    const Slice& name, uint32_t mode, Stat* stat) {
  FilesystemLookupCache::Handle* h = NULL;
  Status status;
  std::string key;
  uint32_t namehash;
  uint32_t hash;
  if (c) {
    key.reserve(16);
    PutFixed64(&key, parent_dir.InodeNo());
    namehash = Hash(name.data(), name.size(), 0);
    PutFixed64(&key, namehash);
    hash = Hash(key.data(), key.size(), 0);
    h = c->lru_.Lookup(key, hash);
  }
  if (!h) {  // Either cache is disabled or key is not cached
    status = Lookup(who, parent_dir, name, mode, stat);
  } else {
    *stat = *h->value;
    c->lru_.Release(h);
    return status;
  }

  if (c && status.ok()) {  // Cache the lookup result if it is a success
    h = c->lru_.Insert(key, hash, new Stat(*stat), 1, DeleteStat);
    c->lru_.Release(h);
  }

  return status;
}

Status Filesystem::Lookup(  ///
    const User& who, const Stat& parent_dir, const Slice& name,
    uint32_t const mode, Stat* const stat) {
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

  stat->SetInodeNo(r_->inoseq++);
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
  const Stat* stat = &r_->rootstat;
  Status status;
  if (!name.empty()) {  // No need to get if target is root
    status = mdb_->Get(pdir, name, &tmp);
    if (!status.ok()) {
      return status;
    } else if (!S_ISDIR(tmp.FileMode())) {  // Must be a dir
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
    : size_lookup_cache(4096),
      skip_name_collision_checks(false),
      skip_perm_checks(false) {}

Filesystem::Filesystem(const FilesystemOptions& options)
    : cache_(NULL), r_(NULL), options_(options), db_(NULL), mdb_(NULL) {
  if (options_.size_lookup_cache) {
    cache_ = new FilesystemLookupCache(options_.size_lookup_cache);
  }
  r_ = new FilesystemRoot;
}

namespace {
inline void FormatFilesystem(Stat* const root) {
  root->SetInodeNo(0);
  root->SetFileSize(0);
  root->SetFileMode(S_IFDIR | S_ISVTX | ACCESSPERMS);
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
  Status status = MDB::Open(dbopts, fsloc, &db_);
  if (!status.ok()) {
    return status;
  }
  mdb_ = new MDB(MDBOptions(db_));
  status = mdb_->LoadFsroot(&tmp);
  if (status.IsNotFound()) {  // This is a new fs image
    FormatFilesystem(&r_->rootstat);
    r_->inoseq = 2;
    status = Status::OK();
  } else if (status.ok()) {
    if (!r_->DecodeFrom(tmp)) {
      status = Status::Corruption("Cannot recover fs root");
    }
  }
  return status;
}

Filesystem::~Filesystem() {
  char tmp[200];
  if (mdb_) {
    Slice encoding = r_->EncodeTo(tmp);
    mdb_->SaveFsroot(encoding);
    delete mdb_;
  }
  delete r_;
  if (cache_) {
    delete cache_;
  }
  if (db_) {
    delete db_;
  }
}

}  // namespace pdlfs
