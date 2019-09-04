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

#include "port_default.h"

namespace pdlfs {

MDBOptions::MDBOptions(DB* db) : db(db) {}

struct MDB::Tx {
  const Snapshot* snap;
  WriteBatch bat;
};

MDB::MDB(const MDBOptions& options) : MXDB(options.db) {}

MDB::~MDB() {}

Status MDB::SaveFsroot(const Slice& encoding) {
  return dx_->Put(WriteOptions(), "/", encoding);
}

Status MDB::LoadFsroot(std::string* tmp) {
  return dx_->Get(ReadOptions(), "/", tmp);
}

Status MDB::Get(const DirId& id, const Slice& fname, Stat* stat) {
  ReadOptions read_opts;
  Tx* const tx = NULL;
  return GET<Key>(id, fname, stat, NULL, &read_opts, tx);
}

Status MDB::Set(const DirId& id, const Slice& fname, const Stat& stat) {
  WriteOptions write_opts;
  Tx* const tx = NULL;
  return SET<Key>(id, fname, stat, fname, &write_opts, tx);
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
