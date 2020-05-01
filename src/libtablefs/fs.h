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

#include "pdlfs-common/fstypes.h"
#include "pdlfs-common/status.h"

#include <stdint.h>
#include <string>

namespace pdlfs {

struct FilesystemLookupCache;
struct FilesystemRoot;

// Options for controlling the filesystem.
struct FilesystemOptions {
  FilesystemOptions();
  size_t size_lookup_cache;  // Default: 0 (cache disabled)
  bool skip_name_collision_checks;
  bool skip_perm_checks;
  bool rdonly;
};
struct FilesystemDir;  // Opaque filesystem dir handle.
// User id information. Each user has a unique id distinguishing them
// from others. In addition, each user can be listed in one or more user groups.
struct User {
  uint32_t uid;
  uint32_t gid;
  // XXX: more groups
};

class FilesystemDb;
// A prototype implementation of TableFS as published in USENIX ATC 2013:
//  - https://www.usenix.org/node/174519
class Filesystem {
 public:
  explicit Filesystem(const FilesystemOptions& options);
  Status OpenFilesystem(const std::string& fsloc);
  ~Filesystem();

  // REQUIRES: OpenFilesystem has been called.
  Status Creat(const User& who, const Stat* at, const char* pathname,
               uint32_t mode);
  Status Mkdir(const User& who, const Stat* at, const char* pathname,
               uint32_t mode);
  Status Lstat(const User& who, const Stat* at, const char* pathname,
               Stat* stat);

  Status Opendir(const User& who, const Stat* at, const char* pathname,
                 FilesystemDir**);
  Status Readdir(FilesystemDir* dir, Stat* stat, std::string* name);
  Status Closdir(FilesystemDir* dir);

 private:
  // Resolve a filesystem path down to the last component of the path. Return
  // the name of the last component and information of its parent directory on
  // success. In addition, return whether the specified path has tailing
  // slashes. This method is a wrapper function over "Resolv", and should be
  // called instead of it. When the input filesystem path points to the root
  // directory, the root directory itself is returned as the parent directory
  // while the name of the last component of the path is set to empty.
  Status Resolu(const User& who, const Stat& at, const char* pathname,
                Stat* parent_dir, Slice* last_component,
                bool* has_tailing_slashes);

  // Resolve a filesystem path down to the last component of the path. Return
  // the name of the last component and information of its parent directory on
  // success. When some parent directory in the middle of the path turns out to
  // not exist, not be a directory, or when other types of error occur, an
  // non-OK status is returned along with the path following (not including) the
  // erroneous directory.
  Status Resolv(const User& who, const Stat& relative_root,
                const char* pathname, Stat* parent_dir, Slice* last_component,
                const char** remaining_path);

  // Retrieve information with the help of an in-mem cache. This function is a
  // wrapper function over "Fetch" which reads data from db. Information is
  // first attempted at the cache before the more costly "Fetch" operation is
  // invoked. When "Fetch" is invoked, the result will be inserted into the
  // cache allowing subsequent lookups to go faster. Set c to NULL when caching
  // is disabled and each lookup hits db.
  Status LookupWithCache(FilesystemLookupCache* const c, const User& who,
                         const Stat& parent_dir, const Slice& name, Stat* stat);

  // Initialize a directory handle for listing.
  Status SeekToDir(const User& who, const Stat& parent_dir, const Slice& name,
                   FilesystemDir** dir);

  // Retrieve information of a name under a given parent directory. If mode is
  // specified, only names of a matching file type (e.g., S_IFDIR, S_IFREG) are
  // considered valid. Set mode to 0 to allow all file types.
  Status Fetch(const User& who, const Stat& parent_dir, const Slice& name,
               uint32_t mode, Stat* stat);

  // Insert a new node beneath a given parent directory. Check name conflicts
  // and return OK and the stat of the newly created node on success.
  Status CheckAndPut(const User& who, const Stat& parent_dir, const Slice& name,
                     uint32_t mode, Stat* stat);

  // Max number of read or write transactions that may go simultaneously. This
  // limit only applies to multi-op transactions. A multi-op transaction
  // performs more than one db or cache accesses, read or write. Single-op
  // transactions consisting of a single db access or a single cache access are
  // not locked at the fs layer and are directly sent to the underlying db or
  // the cache implementation for per-op processing and concurrency control.
  enum { kWay = 8 };  // Must be a power of 2
  port::Mutex mus_[kWay];
  FilesystemLookupCache* cache_;
  port::Mutex rmu_;
  FilesystemRoot* r_;
  // Root encoding of fs at the time fs was opened. This prevents us from
  // writing back root unnecessarily during fs closing.
  std::string prev_r_;

  // No copying allowed
  void operator=(const Filesystem& fs);
  Filesystem(const Filesystem&);
  FilesystemOptions options_;
  FilesystemDb* db_;
};

}  // namespace pdlfs
