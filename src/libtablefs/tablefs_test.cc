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

#include "tablefs.h"

#include "pdlfs-common/random.h"
#include "pdlfs-common/testharness.h"

#include <sys/stat.h>

namespace pdlfs {

class FilesystemTest {
 public:
  FilesystemTest() {
    fsloc_ = test::TmpDir() + "/filesystem_test";
    DestroyDb(fsloc_, MDB::DbOpts());
    fs_ = new Filesystem(options_);
    me.uid = 1;
    me.gid = 1;
  }

  Status Fstat(const char* filename, Stat* stat) {  ///
    return fs_->Fstat(me, NULL, filename, stat);
  }

  Status Creat(const char* filename) {  ///
    return fs_->Creat(me, NULL, filename, 0660);
  }

  ~FilesystemTest() {
    if (fs_) {
      delete fs_;
    }
  }

  User me;
  std::string fsloc_;
  FilesystemOptions options_;
  Filesystem* fs_;
};

TEST(FilesystemTest, OpenFs) {
  ASSERT_OK(fs_->OpenFilesystem(fsloc_));
  delete fs_;
  fs_ = new Filesystem(options_);
  ASSERT_OK(fs_->OpenFilesystem(fsloc_));
}

TEST(FilesystemTest, Files) {
  Stat tmp;
  ASSERT_OK(fs_->OpenFilesystem(fsloc_));
  ASSERT_OK(Creat("/1"));
  ASSERT_CONFLICT(Creat("/1"));
  ASSERT_OK(Fstat("/1", &tmp));
  ASSERT_OK(Fstat("//1", &tmp));
  ASSERT_OK(Fstat("///1", &tmp));
  ASSERT_ERR(Fstat("/1/", &tmp));
  ASSERT_ERR(Fstat("//1//", &tmp));
  ASSERT_NOTFOUND(Fstat("/2", &tmp));
  ASSERT_OK(Creat("/2"));
}

}  // namespace pdlfs

int main(int argc, char** argv) {
  return ::pdlfs::test::RunAllTests(&argc, &argv);
}
