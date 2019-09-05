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
  MDBOptions(::leveldb::DB* db);
  ::leveldb::DB* db;
};

// An MXDB instantiation that binds to Leveldb statically. Leveldb
// is Google's open-source realization of a LSM-Tree.
class MDB : public MXDB<::leveldb::DB, ::leveldb::Slice, ::leveldb::Status,
                        kNameInKey> {
 public:
  // XXX: The DestroyDb port may be a bit too hacky...
#define DestroyDb(x, y) DestroyDB(x, y)  // Remove the contents of a DB
  typedef ::leveldb::Options DbOpts;
  typedef ::leveldb::DB Db;
  static Status Open(const DbOpts&, const std::string& dbloc, Db**);
  explicit MDB(const MDBOptions& options);
  ~MDB();

  Status SaveFsroot(const Slice& encoding);
  Status LoadFsroot(std::string* tmp);

  Status Get(const DirId& id, const Slice& name, Stat* stat);
  Status Set(const DirId& id, const Slice& name, const Stat& stat);
  Status Delete(const DirId& id, const Slice& name);

  typedef MXDB::Dir<::leveldb::Iterator> Dir;
  Dir* Opendir(const DirId& id);
  Status Readdir(Dir* dir, Stat* stat, std::string* name);
  void Closedir(Dir* dir);

 private:
  void operator=(const MDB&);
  MDB(const MDB&);

  struct Tx;
};

}  // namespace pdlfs
