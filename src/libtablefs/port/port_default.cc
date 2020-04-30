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
#include "port_default.h"

namespace pdlfs {

MDBOptions::MDBOptions(DB* db) : db(db) {}

struct MDBStats {
  uint64_t putkeybytes;
  uint64_t putbytes;
  uint64_t puts;
  uint64_t getkeybytes;
  uint64_t getbytes;
  uint64_t gets;
};

struct MDB::Tx {
  const Snapshot* snap;
  WriteBatch bat;
};

MDB::MDB(const MDBOptions& opts) : MXDB(opts.db) {}

MDB::~MDB() {}

Status MDB::Open(const DbOpts& dbopts, const std::string& dbloc, bool rdonly,
                 Db** dbptr) {
  if (rdonly) return ReadonlyDB::Open(dbopts, dbloc, dbptr);
  return Db::Open(dbopts, dbloc, dbptr);
}

Status MDB::SaveFsroot(const Slice& encoding) {
  return dx_->Put(WriteOptions(), "/", encoding);
}

Status MDB::LoadFsroot(std::string* tmp) {
  return dx_->Get(ReadOptions(), "/", tmp);
}

Status MDB::Flush() {  ///
  return dx_->FlushMemTable(FlushOptions());
}

Status MDB::Get(const DirId& id, const Slice& fname, Stat* stat) {
  ReadOptions read_opts;
  Tx* const tx = NULL;
  MDBStats stats;
  return GET<Key>(id, fname, stat, NULL, &read_opts, tx, &stats);
}

Status MDB::Set(const DirId& id, const Slice& fname, const Stat& stat) {
  WriteOptions write_opts;
  Tx* const tx = NULL;
  MDBStats stats;
  return PUT<Key>(id, fname, stat, fname, &write_opts, tx, &stats);
}

Status MDB::Delete(const DirId& id, const Slice& fname) {
  WriteOptions write_opts;
  Tx* const tx = NULL;
  return DELETE<Key>(id, fname, &write_opts, tx);
}

MDB::Dir* MDB::Opendir(const DirId& id) {
  ReadOptions read_opts;
  Tx* const tx = NULL;
  return OPENDIR<Iterator, Key>(id, &read_opts, tx);
}

Status MDB::Readdir(Dir* dir, Stat* stat, std::string* name) {
  return READDIR<Iterator>(dir, stat, name);
}

void MDB::Closedir(MDB::Dir* dir) {
  return CLOSEDIR(dir);  // This also deletes dir
}

}  // namespace pdlfs
