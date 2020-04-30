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
#include "../tablefs_db.h"

#include "pdlfs-common/env.h"
#include "pdlfs-common/fsdbx.h"
#include "pdlfs-common/fstypes.h"
#include "pdlfs-common/leveldb/db.h"
#include "pdlfs-common/leveldb/readonly.h"
#include "pdlfs-common/leveldb/snapshot.h"
#include "pdlfs-common/leveldb/write_batch.h"
#include "pdlfs-common/status.h"

namespace pdlfs {
namespace port {
// An MXDB template instantiation that binds to our own DB implementation. Our
// DB is a modified LevelDB realization of an LSM-tree. Our modification enables
// deeper LSM-tree customization, true readonly db access, and bulk sstable
// insertion operation.
typedef MXDB<DB, Slice, Status, kNameInKey> MDB;
}  // namespace port
struct FilesystemDb::Rep {
  Rep();
  port::MDB* mdb;
  DB* db;
};
namespace {
Status OpenDb(const FilesystemOptions& options, const std::string& dbloc,
              DB** db) {
  DBOptions dbopts;  // XXX: filter? block cache? table cache?
  dbopts.create_if_missing = !options.rdonly;
  dbopts.disable_seek_compaction = true;
  dbopts.skip_lock_file = true;
  if (options.rdonly) return ReadonlyDB::Open(dbopts, dbloc, db);
  return DB::Open(dbopts, dbloc, db);
}
struct Tx {  // Db transaction. Not used, but required by the MXDB code.
  const Snapshot* snap;
  WriteBatch bat;
};
Tx* const NULLTX = NULL;
}  // namespace

Status FilesystemDb::Open(const std::string& dbloc) {
  Status s = OpenDb(options_, dbloc, &rep_->db);
  if (s.ok()) {
    rep_->mdb = new port::MDB(rep_->db);
  }
  return s;
}

Status FilesystemDb::SaveFsroot(const Slice& root_encoding) {
  return rep_->db->Put(WriteOptions(), "/", root_encoding);
}

Status FilesystemDb::LoadFsroot(std::string* tmp) {
  return rep_->db->Get(ReadOptions(), "/", tmp);
}

Status FilesystemDb::Flush() { return rep_->db->FlushMemTable(FlushOptions()); }

Status FilesystemDb::Get(const DirId& id, const Slice& fname, Stat* stat,
                         FilesystemDbStats* stats) {
  ReadOptions myreadopts;
  return rep_->mdb->GET<Key>(id, fname, stat, NULL, &myreadopts, NULLTX, stats);
}

Status FilesystemDb::Put(const DirId& id, const Slice& fname, const Stat& stat,
                         FilesystemDbStats* stats) {
  WriteOptions mywriteopts;
  return rep_->mdb->PUT<Key>(id, fname, stat, fname, &mywriteopts, NULLTX,
                             stats);
}

Status FilesystemDb::Delete(const DirId& id, const Slice& fname) {
  WriteOptions myopts;
  return rep_->mdb->DELETE<Key>(id, fname, &myopts, NULLTX);
}

FilesystemDb::Dir* FilesystemDb::Opendir(const DirId& dir_id) {
  ReadOptions myreadopts;
  return reinterpret_cast<Dir*>(
      rep_->mdb->OPENDIR<Iterator, Key>(dir_id, &myreadopts, NULLTX));
}

Status FilesystemDb::Readdir(Dir* dir, Stat* stat, std::string* name) {
  return rep_->mdb->READDIR<Iterator>(
      reinterpret_cast<port::MDB::Dir<Iterator>*>(dir), stat, name);
}

void FilesystemDb::Closedir(Dir* dir) {
  return rep_->mdb->CLOSEDIR(reinterpret_cast<port::MDB::Dir<Iterator>*>(dir));
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
