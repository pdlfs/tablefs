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
#include "../fsdb.h"

#include <kvrangedb/db.h>
#include <kvrangedb/options.h>
#include <kvrangedb/slice.h>
#include <kvrangedb/status.h>
#include <kvrangedb/write_batch.h>

namespace pdlfs {
namespace port {
// An MXDB instantiation that binds to KVRANGEDB. KVRANGEDB is a custom B-Tree
// implementation supporting range queries atop KVSSD through storing a
// secondary ordered key index in the KVSSD device. KVRANGEDB is a LANL research
// project - https://github.com/celeryfake/kvrangedb
typedef MXDB<::kvrangedb::DB, ::kvrangedb::Slice, ::kvrangedb::Status,
             kNameInKey>
    MDB;
}  // namespace port
struct FilesystemDb::Rep {
  Rep();
  port::MDB* mdb;
  ::kvrangedb::DB* db;
};
namespace {
struct ReadOptions2 : public ::kvrangedb::ReadOptions {
  ReadOptions2() : ReadOptions(), snapshot(NULL) {}
  const void* snapshot;
};
::kvrangedb::Status OpenDb(  ///
    const FilesystemOptions& options, const std::string& dbloc,
    ::kvrangedb::DB** db) {
  ::kvrangedb::Options dbopts;  // XXX: add kvrangedb specific configurations
  return ::kvrangedb::DB::Open(dbopts, dbloc, db);
}
struct Tx {  // Db transaction. Not used, but required by the MXDB code.
  const void* snap;
  ::kvrangedb::WriteBatch bat;
};
Tx* const NULLTX = NULL;
}  // namespace

Status FilesystemDb::Open(const std::string& dbloc) {
  ::kvrangedb::Status status = OpenDb(options_, dbloc, &rep_->db);
  if (!status.ok()) {
    return Status::IOError(status.ToString());
  }
  rep_->mdb = new port::MDB(rep_->db);
  return Status::OK();
}

Status FilesystemDb::SaveFsroot(const Slice& root_encoding) {
  ::kvrangedb::Status status = rep_->db->Put(
      ::kvrangedb::WriteOptions(), "/",
      ::kvrangedb::Slice(root_encoding.data(), root_encoding.size()));
  if (!status.ok()) {
    return Status::IOError(status.ToString());
  } else {
    return Status::OK();
  }
}

Status FilesystemDb::LoadFsroot(std::string* tmp) {
  ::kvrangedb::Status status = rep_->db->Get(ReadOptions2(), "/", tmp);
  if (status.IsNotFound()) {
    return Status::NotFound(Slice());
  } else if (!status.ok()) {
    return Status::IOError(status.ToString());
  } else {
    return Status::OK();
  }
}

Status FilesystemDb::Flush() { return Status::OK(); }

Status FilesystemDb::Get(const DirId& id, const Slice& fname, Stat* stat,
                         FilesystemDbStats* stats) {
  ReadOptions2 myreadopts;
  return rep_->mdb->GET<Key>(id, fname, stat, NULL, &myreadopts, NULLTX, stats);
}

Status FilesystemDb::Put(const DirId& id, const Slice& fname, const Stat& stat,
                         FilesystemDbStats* stats) {
  ::kvrangedb::WriteOptions mywriteopts;
  return rep_->mdb->PUT<Key>(id, fname, stat, fname, &mywriteopts, NULLTX,
                             stats);
}

Status FilesystemDb::Delete(const DirId& id, const Slice& fname) {
  ::kvrangedb::WriteOptions myopts;
  return rep_->mdb->DELETE<Key>(id, fname, &myopts, NULLTX);
}

FilesystemDb::Dir* FilesystemDb::Opendir(const DirId& dir_id) {
  ReadOptions2 myreadopts;
  return reinterpret_cast<Dir*>(rep_->mdb->OPENDIR<::kvrangedb::Iterator, Key>(
      dir_id, &myreadopts, NULLTX));
}

Status FilesystemDb::Readdir(Dir* dir, Stat* stat, std::string* name) {
  return rep_->mdb->READDIR<::kvrangedb::Iterator>(
      reinterpret_cast<port::MDB::Dir<::kvrangedb::Iterator>*>(dir), stat,
      name);
}

void FilesystemDb::Closedir(Dir* dir) {
  return rep_->mdb->CLOSEDIR(
      reinterpret_cast<port::MDB::Dir<::kvrangedb::Iterator>*>(dir));
}

FilesystemDb::FilesystemDb(const FilesystemOptions& options)
    : options_(options), rep_(new Rep()) {}

FilesystemDb::Rep::Rep() : mdb(NULL), db(NULL) {}

FilesystemDb::~FilesystemDb() {
  delete rep_->mdb;
  delete rep_->db;
  delete rep_;
}

}  // namespace pdlfs
