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

namespace pdlfs {

class FilesystemTest {
 public:
  FilesystemTest() {
    fsloc_ = test::TmpDir() + "/filesystem_test";
    DestroyDb(fsloc_, MDB::DbOpts());
    fs_ = new Filesystem(options_);
  }

  ~FilesystemTest() {
    if (fs_) {
      delete fs_;
    }
  }

  std::string fsloc_;
  FilesystemOptions options_;
  Filesystem* fs_;
};

TEST(FilesystemTest, Open) {  ///
  ASSERT_OK(fs_->OpenFilesystem(fsloc_));
}

}  // namespace pdlfs

int main(int argc, char** argv) {
  return ::pdlfs::test::RunAllTests(&argc, &argv);
}
