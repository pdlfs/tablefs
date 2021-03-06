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
#include "fs.h"

#include "fsdb.h"

#include "pdlfs-common/lru.h"
#include "pdlfs-common/mutexlock.h"

#include <sys/stat.h>

namespace pdlfs {

// Lookup cache for speeding up pathname resolutions.
struct FilesystemLookupCache {
  explicit FilesystemLookupCache(size_t cap) : lru_(cap) {}
  typedef LRUEntry<Stat> Handle;
  LRUCache<Handle> lru_;
  port::Mutex mu_;
};

// Root information of a filesystem image.
struct FilesystemRoot {
  FilesystemRoot() {}  // Intentionally not initialized for performance
  // Inode num for the next file or directory
  uint64_t inoseq_;
  // Stat of the root directory
  Stat rstat_;
};

Status Filesystem::Lstat(  ///
    const User& who, const char* const pathname, Stat* const stat,
    FilesystemDbStats* const stats) {
  bool has_tailing_slashes(false);
  Stat parent_dir;
  Slice tgt;
  Status status = Resolu(who, r_->rstat_, pathname, &parent_dir, &tgt,
                         &has_tailing_slashes, stats);
  if (!status.ok()) {
    return status;
  }

  if (!tgt.empty()) {
    const uint32_t mode =
        has_tailing_slashes ? S_IFDIR : 0;  // Target must be a dir
    status = Fetch(who, parent_dir, tgt, mode, stat, stats);
  } else {  // Special case in which path is a root
    *stat = r_->rstat_;
  }

  return status;
}

Status Filesystem::Opendir(  ///
    const User& who, const char* const pathname, FilesystemDir** const dir,
    FilesystemDbStats* const stats) {
  bool has_tailing_slashes(false);
  Stat parent_dir;
  Slice tgt;
  Status status = Resolu(who, r_->rstat_, pathname, &parent_dir, &tgt,
                         &has_tailing_slashes, stats);
  if (!status.ok()) {
    return status;
  }

  status = SeekToDir(who, parent_dir, tgt, dir, stats);

  return status;
}

Status Filesystem::Readdir(  ///
    FilesystemDir* dir, Stat* stat, std::string* name) {
  FilesystemDb::Dir* d = reinterpret_cast<FilesystemDb::Dir*>(dir);
  return db_->Readdir(d, stat, name);
}

Status Filesystem::Closdir(FilesystemDir* dir) {
  FilesystemDb::Dir* d = reinterpret_cast<FilesystemDb::Dir*>(dir);
  db_->Closedir(d);
  return Status::OK();
}

Status Filesystem::Rmdir(  ///
    const User& who, const char* const pathname,
    FilesystemDbStats* const stats) {
  bool has_tailing_slashes(false);
  Stat parent_dir;
  Slice tgt;
  Status status = Resolu(who, r_->rstat_, pathname, &parent_dir, &tgt,
                         &has_tailing_slashes, stats);
  if (!status.ok()) {
    return status;
  } else if (tgt.empty()) {  // Special case in which path is a root
    return Status::AssertionFailed(Slice());
  }

  Stat stat;
  status = RemoveDir(who, parent_dir, tgt, &stat, stats);

  return status;
}

Status Filesystem::Mkdir(  ///
    const User& who, const char* const pathname, uint32_t mode,
    FilesystemDbStats* const stats) {
  bool has_tailing_slashes(false);
  Stat parent_dir;
  Slice tgt;
  Status status = Resolu(who, r_->rstat_, pathname, &parent_dir, &tgt,
                         &has_tailing_slashes, stats);
  if (!status.ok()) {
    return status;
  } else if (tgt.empty()) {  // Special case in which path is a root
    return Status::AlreadyExists(Slice());
  }

  mode = S_IFDIR | (ALLPERMS & mode);
  Stat stat;
  status = Put(who, parent_dir, tgt, mode, &stat, stats);

  return status;
}

Status Filesystem::Unlnk(  ///
    const User& who, const char* pathname, FilesystemDbStats* const stats) {
  bool has_tailing_slashes(false);
  Stat parent_dir;
  Slice tgt;
  Status status = Resolu(who, r_->rstat_, pathname, &parent_dir, &tgt,
                         &has_tailing_slashes, stats);
  if (!status.ok()) {
    return status;
  } else if (tgt.empty()) {  // Special case in which path is a root
    return Status::FileExpected(Slice());
  } else if (has_tailing_slashes) {  // Path is a dir
    return Status::FileExpected(Slice());
  }

  Stat stat;
  status = Delete(who, parent_dir, tgt, &stat, stats);

  return status;
}

Status Filesystem::Creat(  ///
    const User& who, const char* pathname, uint32_t mode,
    FilesystemDbStats* const stats) {
  bool has_tailing_slashes(false);
  Stat parent_dir;
  Slice tgt;
  Status status = Resolu(who, r_->rstat_, pathname, &parent_dir, &tgt,
                         &has_tailing_slashes, stats);
  if (!status.ok()) {
    return status;
  } else if (tgt.empty()) {  // Special case in which path is a root
    return Status::AlreadyExists(Slice());
  } else if (has_tailing_slashes) {  // Path is a dir
    return Status::FileExpected(Slice());
  }

  mode = S_IFREG | (ALLPERMS & mode);
  Stat stat;
  status = Put(who, parent_dir, tgt, mode, &stat, stats);

  return status;
}

Status Filesystem::Resolu(  ///
    const User& who, const Stat& at, const char* const pathname,
    Stat* parent_dir, Slice* last_component,  ///
    bool* has_tailing_slashes, FilesystemDbStats* stats) {
#define PATH_SGMT(pathname, remaining_path) \
  Slice(pathname, remaining_path - pathname)
  const char* remaining_path(NULL);
  Status status = Resolv(who, at, pathname, parent_dir, last_component,
                         &remaining_path, stats);
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
    Stat* const parent_dir, Slice* const last_component,
    const char** const remaining_path,  ///
    FilesystemDbStats* const stats) {
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
    status = LookupWithCache(cache_, who, *current_parent, current_name, &tmp,
                             stats);
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
inline Slice LookupKey(char* const dst, const DirId& parent_dir,
                       const Slice& name) {
  char* p = dst;
  EncodeFixed64(p, parent_dir.ino);
  p += 8;
  EncodeFixed32(p, Hash(name.data(), name.size(), 0));
  p += 4;
  return Slice(dst, p - dst);
}

inline uint32_t Hash0(const Slice& key) {
  return Hash(key.data(), key.size(), 0);
}

void DeleteStat(  /// Delete stat inside a cache entry
    const Slice& key, Stat* stat) {
  delete stat;
}
}  // namespace

Status Filesystem::LookupWithCache(  ///
    FilesystemLookupCache* const c, const User& who, const Stat& parent_dir,
    const Slice& name, Stat* const stat, FilesystemDbStats* const stats) {
  FilesystemLookupCache::Handle* h = NULL;
  Status status;
  char tmp[30];
  port::Mutex* mu = NULL;
  uint32_t hash;
  Slice key;
  if (c) {
    key = LookupKey(tmp, DirId(parent_dir), name);
    hash = Hash0(key);
    // Mutex locking is only needed when cache is enabled so we must ensure that
    // the group of a cache lookup operation, a db fetch operation, and a cache
    // insertion operation go as a single atomic operation.
    mu = &mus_[hash & (kWay - 1)];
    mu->Lock();
    MutexLock cl(&c->mu_);
    h = c->lru_.Lookup(key, hash);
    if (h) {  // Key is in cache; use it!
      *stat = *h->value;
      c->lru_.Release(h);
    }
  }
  if (!h) {  // Either cache is disabled or key is not in cache
    status = Fetch(who, parent_dir, name, S_IFDIR, stat, stats);
    if (c && status.ok()) {
      // Cache result if it is a success.
      MutexLock cl(&c->mu_);
      h = c->lru_.Insert(key, hash, new Stat(*stat), 1, DeleteStat);
      c->lru_.Release(h);
    }
  }

  if (mu) {
    mu->Unlock();
  }

  return status;
}

Status Filesystem::Fetch(  ///
    const User& who, const Stat& parent_dir, const Slice& name, uint32_t mode,
    Stat* const stat, FilesystemDbStats* const stats) {
  if (!IsLookupOk(options_, parent_dir, who)) {
    return Status::AccessDenied(Slice());
  }
  Status status = db_->Get(DirId(parent_dir), name, stat, stats);
  if (!status.ok()) {
    return status;
  } else if ((stat->FileMode() & mode) != mode) {
    return UnexpectedMode(mode);
  } else {
    return status;
  }
}

Status Filesystem::RemoveDir(  ///
    const User& who, const Stat& parent_dir, const Slice& name,
    Stat* const stat, FilesystemDbStats* const stats) {
  if (!IsDirWriteOk(options_, parent_dir, who)) {
    return Status::AccessDenied(Slice());
  }
  const DirId pdir(parent_dir);
  Status status;
  const bool use_mu = !options_.skip_deletion_checks;
  if (use_mu) {
    for (int i = 0; i < kWay; i++) {
      mus_[i].Lock();
    }
    status = db_->Get(pdir, name, stat, stats);
    if (status.ok() && (stat->FileMode() & S_IFDIR) != S_IFDIR) {
      status = Status::DirExpected(Slice());
    }
    {
      FilesystemDb::Dir* const dir = db_->Opendir(DirId(*stat));
      std::string tmpname;
      Stat tmp;
      Status ss = db_->Readdir(dir, &tmp, &tmpname);
      if (ss.ok()) {
        status = Status::DirNotEmpty(Slice());
      } else if (status.IsNotFound()) {
        status = Status::OK();
      }
      db_->Closedir(dir);
    }
  }

  if (status.ok()) {
    status = db_->Delete(pdir, name);
    FilesystemLookupCache* const c = cache_;
    if (c && status.ok()) {
      char tmp[30];
      Slice key = LookupKey(tmp, pdir, name);
      uint32_t hash = Hash0(key);
      MutexLock cl(&c->mu_);
      c->lru_.Erase(key, hash);
    }
  }

  if (use_mu) {
    for (int i = kWay - 1; i >= 0; i--) {
      mus_[i].Unlock();
    }
  }

  return status;
}

Status Filesystem::Delete(  ///
    const User& who, const Stat& parent_dir, const Slice& name,
    Stat* const stat, FilesystemDbStats* const stats) {
  if (!IsDirWriteOk(options_, parent_dir, who)) {
    return Status::AccessDenied(Slice());
  }
  const DirId pdir(parent_dir);
  Status status;
  char tmp[30];
  port::Mutex* mu = NULL;
  uint32_t hash;
  Slice key;
  if (!options_.skip_deletion_checks) {
    key = LookupKey(tmp, pdir, name);
    hash = Hash0(key);
    // Mutex locking is needed when name existence must be checked prior to
    // deletion.
    mu = &mus_[hash & (kWay - 1)];
    mu->Lock();
    status = db_->Get(pdir, name, stat, stats);
    if (status.ok() && (stat->FileMode() & S_IFREG) != S_IFREG) {
      status = Status::FileExpected(Slice());
    }
  }

  if (status.ok()) {
    status = db_->Delete(pdir, name);
  }

  if (mu) {
    mu->Unlock();
  }

  return status;
}

Status Filesystem::Put(  ///
    const User& who, const Stat& parent_dir, const Slice& name, uint32_t mode,
    Stat* const stat, FilesystemDbStats* const stats) {
  if (!IsDirWriteOk(options_, parent_dir, who)) {
    return Status::AccessDenied(Slice());
  }
  const DirId pdir(parent_dir);
  Status status;
  char tmp[30];
  port::Mutex* mu = NULL;
  uint32_t hash;
  Slice key;
  if (!options_.skip_name_collision_checks) {
    key = LookupKey(tmp, pdir, name);
    hash = Hash0(key);
    // Mutex locking is needed when we have to do a read before writing.
    mu = &mus_[hash & (kWay - 1)];
    mu->Lock();
    status = db_->Get(pdir, name, stat, stats);
    if (status.ok()) {
      status = Status::AlreadyExists(Slice());
    } else if (status.IsNotFound()) {
      status = Status::OK();
    }
  }

  if (status.ok()) {
    rmu_.Lock();
    stat->SetInodeNo(r_->inoseq_++);
    rmu_.Unlock();
    stat->SetFileSize(0);
    stat->SetFileMode(mode);
    stat->SetUserId(who.uid);
    stat->SetGroupId(who.gid);
    stat->SetModifyTime(0);
    stat->SetChangeTime(0);
    stat->AssertAllSet();

    status = db_->Put(pdir, name, *stat, stats);
  }

  if (mu) {
    mu->Unlock();
  }

  return status;
}

Status Filesystem::SeekToDir(  ///
    const User& who, const Stat& parent_dir, const Slice& name,
    FilesystemDir** const dir, FilesystemDbStats* const stats) {
  if (!IsLookupOk(options_, parent_dir, who)) {
    return Status::AccessDenied(Slice());
  }
  Stat buf;
  const DirId pdir(parent_dir);
  const Stat* stat = &r_->rstat_;
  Status status;
  char tmp[30];
  port::Mutex* mu = NULL;
  uint32_t hash;
  Slice key;
  if (!name.empty()) {  // No need to check if name is root
    key = LookupKey(tmp, pdir, name);
    hash = Hash0(key);
    // Mutex locking is needed when we need to perform two db reads.
    mu = &mus_[hash & (kWay - 1)];
    mu->Lock();
    status = db_->Get(pdir, name, &buf, stats);
    if (!status.ok()) {
      // Empty
    } else if (!S_ISDIR(buf.FileMode())) {  // Must be a dir
      status = Status::DirExpected(Slice());
    } else {
      stat = &buf;
    }
  }

  if (status.ok()) {
    if (IsDirReadOk(options_, *stat, who)) {
      *dir = reinterpret_cast<FilesystemDir*>(db_->Opendir(DirId(*stat)));
    } else {
      status = Status::AccessDenied(Slice());
    }
  }

  if (mu) {
    mu->Unlock();
  }

  return status;
}

uint64_t Filesystem::TEST_GetCurrentInoseq() {
  MutexLock ml(&rmu_);
  return r_->inoseq_;
}

namespace {
// Recover information from a given encoding string.
// Return True on success, False otherwise.
bool DecodeFrom(FilesystemRoot* r, Slice* input) {
  if (!r->rstat_.DecodeFrom(input))  ///
    return false;
  return GetVarint64(input, &r->inoseq_);
}

bool DecodeFrom(FilesystemRoot* r, const Slice& encoding) {
  Slice input = encoding;
  return DecodeFrom(r, &input);
}

// Encode root information into a buf space.
Slice EncodeTo(FilesystemRoot* r, char* scratch) {
  Slice en = r->rstat_.EncodeTo(scratch);
  char* p = EncodeVarint64(scratch + en.size(), r->inoseq_);
  Slice rv(scratch, p - scratch);
  return rv;
}
}  // namespace

FilesystemOptions::FilesystemOptions()
    : size_lookup_cache(0),
      skip_deletion_checks(false),
      skip_name_collision_checks(false),
      skip_perm_checks(false),
      rdonly(false) {}

Filesystem::Filesystem(const FilesystemOptions& options)
    : cache_(NULL), r_(NULL), options_(options), db_(NULL) {
  if (options_.size_lookup_cache) {
    cache_ = new FilesystemLookupCache(options_.size_lookup_cache);
  }
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
  db_ = new FilesystemDb(options_);
  Status s = db_->Open(fsloc);
  if (s.ok()) {
    s = db_->LoadFsroot(&prev_r_);
    if (s.IsNotFound()) {  // This is a new fs image
      r_ = new FilesystemRoot;
      FormatFilesystem(&r_->rstat_);
      r_->inoseq_ = 1;
      s = Status::OK();
    } else if (s.ok()) {
      r_ = new FilesystemRoot;
      if (!DecodeFrom(r_, prev_r_)) {
        s = Status::Corruption("Cannot recover fs root");
      }
    }
  }
  // We indicate error by deleting db_ and r_ and setting them to NULL.
  if (!s.ok()) {
    delete db_;
    db_ = NULL;
    delete r_;
    r_ = NULL;
  }
  return s;
}

Filesystem::~Filesystem() {
  char tmp[200];
  if (!options_.rdonly && r_ && db_) {
    Slice encoding = EncodeTo(r_, tmp);
    if (encoding != prev_r_) {
      db_->SaveFsroot(encoding);
    }
    db_->Flush();
  }
  delete cache_;
  delete db_;
  delete r_;
}

}  // namespace pdlfs
