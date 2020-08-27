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

#include "fs.h"

#include "pdlfs-common/fsdb0.h"

#include <stdint.h>

namespace pdlfs {

// Db performance stats.
struct FilesystemDbStats {
  FilesystemDbStats();
  // Total amount of key bytes pushed to db.
  uint64_t putkeybytes;
  // Total amount of val bytes pushed to db.
  uint64_t putbytes;
  // Total number of put operations.
  uint64_t puts;
  // Total number of key bytes read out of db.
  uint64_t getkeybytes;
  // Total number of val bytes read out of db.
  uint64_t getbytes;
  // Total number of get operations.
  uint64_t gets;
};

class FilesystemDb {
 public:
  explicit FilesystemDb(const FilesystemOptions& options);
  ~FilesystemDb();

  Status Open(const std::string& dbloc);
  Status SaveFsroot(const Slice& root_encoding);
  Status LoadFsroot(std::string* tmp);
  Status Flush();

  Status Get(const DirId& parent, const Slice& name, Stat* stat,
             FilesystemDbStats* stats);
  Status Put(const DirId& parent, const Slice& name, const Stat& stat,
             FilesystemDbStats* stats);
  Status Delete(const DirId& parent, const Slice& name);

  struct Dir;
  Dir* Opendir(const DirId& dir_id);
  Status Readdir(Dir* dir, Stat* stat, std::string* name);
  void Closedir(Dir* dir);

 private:
  void operator=(const FilesystemDb& fsdb);  // No copying allowed
  FilesystemDb(const FilesystemDb&);

  FilesystemOptions options_;

  struct Rep;
  Rep* rep_;
};

}  // namespace pdlfs
