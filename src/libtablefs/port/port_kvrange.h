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

#include <kvrangedb/db.h>
#include <kvrangedb/options.h>
#include <kvrangedb/slice.h>
#include <kvrangedb/status.h>
#include <kvrangedb/write_batch.h>

namespace pdlfs {

// Options for controlling MDB behavior.
struct MDBOptions {
  ::kvrangedb::DB* db;
  MDBOptions();
};

// An MXDB instantiation that binds to KVRANGEDB. KVRANGEDB is a custom B-Tree
// implementation supporting range queries atop KVSSD through storing a
// secondary ordered key index in the KVSSD device. KVRANGEDB is a LANL research
// project. https://github.com/celeryfake/kvrangedb
class MDB : public MXDB<::kvrangedb::DB, ::kvrangedb::Slice,
                        ::kvrangedb::Status, kNameInKey> {
 public:
  typedef ::kvrangedb::DB Db;
  explicit MDB(const MDBOptions& options);
  ~MDB();

  Status Get(const DirId& id, const Slice& name, Stat* stat);
  Status Set(const DirId& id, const Slice& name, const Stat& stat);
  Status Delete(const DirId& id, const Slice& name);

  typedef MXDB::Dir<::kvrangedb::Iterator> Dir;
  Dir* Opendir(const DirId& id);
  Status Readdir(Dir* dir, Stat* stat, std::string* name);
  void Closedir(Dir* dir);

 private:
  void operator=(const MDB&);
  MDB(const MDB&);

  struct Tx;
};

}  // namespace pdlfs