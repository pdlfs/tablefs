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

#include "port.h"

#include "pdlfs-common/fsdbx.h"
#include "pdlfs-common/port.h"

#include <string>

namespace pdlfs {

// Options for controlling the filesystem.
struct FilesystemOptions {
  FilesystemOptions();
  bool skip_name_collision_checks;
  bool skip_perm_checks;
};
// Root information of a filesystem (superblock).
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
struct FilesystemDir;  // Abstract filesystem dir handle.
// User id information.
struct User {
  uid_t uid;
  gid_t gid;
};

class Filesystem {
 public:
  explicit Filesystem(const FilesystemOptions& options);
  ~Filesystem();

  Status OpenFilesystem(const std::string& fsloc);

  Status Creat(const User& who, const Stat* at, const char* pathname,
               uint32_t mode);
  Status Mkdir(const User& who, const Stat* at, const char* pathname,
               uint32_t mode);

  Status Opendir(const User& who, const Stat* at, const char* pathname,
                 FilesystemDir**);
  Status Readdir(FilesystemDir* dir, Stat* stat, std::string* name);
  Status Closdir(FilesystemDir* dir);

 private:
  Status Resolu(const User& who, const Stat& at, const char* pathname,
                Stat* parent_dir, Slice* last_component,
                bool* has_tailing_slashes);
  Status Resolv(const User& who, const Stat& relative_root,
                const char* pathname, Stat* parent_dir, Slice* last_component,
                const char** remaining_path);

  Status Lookup(const User& who, const Stat& parent_dir, const Slice& name,
                uint32_t mode, Stat* stat);
  Status Getdir(const User& who, const Stat& parent_dir, const Slice& name,
                FilesystemDir** dir);

  // Insert a new filesystem node beneath a given parent directory.
  // Return OK and the stat of the newly created filesystem node on success.
  Status Insert(const User& who, const Stat& parent_dir, const Slice& name,
                uint32_t mode, Stat* stat);

  // No copying allowed
  void operator=(const Filesystem& fs);
  Filesystem(const Filesystem&);

  FilesystemRoot r_;
  FilesystemOptions options_;
  MDB::Db* db_;
  MDB* mdb_;
};

}  // namespace pdlfs
