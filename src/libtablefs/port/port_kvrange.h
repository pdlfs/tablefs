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
