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

#include "pdlfs-common/mdb.h"

#include <leveldb/db.h>
#include <leveldb/options.h>
#include <leveldb/slice.h>
#include <leveldb/status.h>
#include <leveldb/write_batch.h>

namespace pdlfs {

// Options for controlling MDB behavior.
struct MDBOptions {
  ::leveldb::DB* db;
  MDBOptions();
};

// An MXDB instantiation that binds to Leveldb. Leveldb is Google's
// open-source realization of a LSM-Tree.
class MDB : public MXDB<::leveldb::DB, ::leveldb::Slice, ::leveldb::Status,
                        kNameInKey> {
 public:
  struct Tx;
  explicit MDB(const MDBOptions& options);
  ~MDB();

  Status Get(const DirId& id, const Slice& fname, Stat* stat);
  Status Set(const DirId& id, const Slice& fname, const Stat& stat);
  Status Delete(const DirId& id, const Slice& fname);

  typedef MXDB::Dir<::leveldb::Iterator> Dir;
  Dir* Opendir(const DirId& id);
  Status Readdir(Dir* dir, Stat* stat, std::string* name);
  void Closedir(Dir* dir);

 private:
  void operator=(const MDB&);
  MDB(const MDB&);
};

}  // namespace pdlfs
