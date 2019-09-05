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

#include "port_leveldb.h"

namespace pdlfs {

MDBOptions::MDBOptions(::leveldb::DB* db) : db(db) {}

struct MDB::Tx {
  const ::leveldb::Snapshot* snap;
  ::leveldb::WriteBatch bat;
};

MDB::MDB(const MDBOptions& options) : MXDB(options.db) {}

MDB::~MDB() {}

Status MDB::Open(const DbOpts& dbopts, const std::string& dbloc, Db** dbptr) {
  ::leveldb::Status status = Db::Open(dbopts, dbloc, dbptr);
  if (!status.ok()) {
    return Status::IOError(status.ToString());
  } else {
    return Status::OK();
  }
}

Status MDB::SaveFsroot(const Slice& encoding) {
  ::leveldb::Status status =
      dx_->Put(::leveldb::WriteOptions(), "/",
               ::leveldb::Slice(encoding.data(), encoding.size()));
  if (!status.ok()) {
    return Status::IOError(status.ToString());
  } else {
    return Status::OK();
  }
}

Status MDB::LoadFsroot(std::string* tmp) {
  ::leveldb::Status status =  ///
      dx_->Get(::leveldb::ReadOptions(), "/", tmp);
  if (status.IsNotFound()) {
    return Status::NotFound(Slice());
  } else if (!status.ok()) {
    return Status::IOError(status.ToString());
  } else {
    return Status::OK();
  }
}

Status MDB::Get(const DirId& id, const Slice& fname, Stat* stat) {
  ::leveldb::ReadOptions ropts;
  Tx* const tx = NULL;
  return GET<Key>(id, fname, stat, NULL, &ropts, tx);
}

Status MDB::Set(const DirId& id, const Slice& fname, const Stat& stat) {
  ::leveldb::WriteOptions wopts;
  Tx* const tx = NULL;
  return SET<Key>(id, fname, stat, fname, &wopts, tx);
}

Status MDB::Delete(const DirId& id, const Slice& fname) {
  ::leveldb::WriteOptions wopts;
  Tx* const tx = NULL;
  return DELETE<Key>(id, fname, &wopts, tx);
}

MDB::Dir* MDB::Opendir(const DirId& id) {
  ::leveldb::ReadOptions ropts;
  Tx* const tx = NULL;
  return OPENDIR<::leveldb::Iterator, Key>(id, &ropts, tx);
}

Status MDB::Readdir(Dir* dir, Stat* stat, std::string* name) {
  return READDIR<::leveldb::Iterator>(dir, stat, name);
}

void MDB::Closedir(MDB::Dir* dir) {
  return CLOSEDIR(dir);  // This also deletes dir
}

}  // namespace pdlfs
