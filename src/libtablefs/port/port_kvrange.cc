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

#include "port_kvrange.h"

namespace pdlfs {

MDBOptions::MDBOptions() : db(NULL) {}

struct ReadOptions2 : public ::kvrangedb::ReadOptions {
  ReadOptions2() : ReadOptions(), snap(NULL) {}
  void* snap;
};

struct MDB::Tx {
  ::kvrangedb::WriteBatch bat;
  void* snap;
};

MDB::MDB(const MDBOptions& options) : MXDB(options.db) {}

MDB::~MDB() {}

Status MDB::Get(const DirId& id, const Slice& fname, Stat* stat) {
  ReadOptions2 ropts;
  Tx* const tx = NULL;
  return GET<Key>(id, fname, stat, NULL, &ropts, tx);
}

Status MDB::Set(const DirId& id, const Slice& fname, const Stat& stat) {
  ::kvrangedb::WriteOptions wopts;
  Tx* const tx = NULL;
  return SET<Key>(id, fname, stat, fname, &wopts, tx);
}

Status MDB::Delete(const DirId& id, const Slice& fname) {
  ::kvrangedb::WriteOptions wopts;
  Tx* const tx = NULL;
  return DELETE<Key>(id, fname, &wopts, tx);
}

MDB::Dir* MDB::Opendir(const DirId& id) {
  ReadOptions2 ropts;
  Tx* const tx = NULL;
  return OPENDIR<::kvrangedb::Iterator, Key>(id, &ropts, tx);
}

Status MDB::Readdir(Dir* dir, Stat* stat, std::string* name) {
  return READDIR<::kvrangedb::Iterator>(dir, stat, name);
}

void MDB::Closedir(MDB::Dir* dir) {
  return CLOSEDIR(dir);  // This also deletes dir
}

}  // namespace pdlfs
