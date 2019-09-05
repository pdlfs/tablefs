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
#include <set>

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

  void Listdir(FilesystemDir* dir, std::set<std::string>* set) {
    Status status;
    std::string name;
    Stat tmp;
    while (true) {
      status = fs_->Readdir(dir, &tmp, &name);
      if (status.ok()) {
        set->insert(name);
      } else {
        break;
      }
    }
  }

  Status Exist(const char* path, const Stat* at = NULL) {
    Stat ignored;
    return fs_->Lstat(me, at, path, &ignored);
  }

  Status Creat(const char* path, const Stat* at = NULL) {
    return fs_->Mkfil(me, at, path, 0660);
  }

  Status Mkdir(const char* path, const Stat* at = NULL) {
    return fs_->Mkdir(me, at, path, 0770);
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
  ASSERT_OK(Exist("/"));
  ASSERT_OK(Exist("//"));
  ASSERT_OK(Exist("///"));
  delete fs_;
  fs_ = new Filesystem(options_);
  ASSERT_OK(fs_->OpenFilesystem(fsloc_));
  ASSERT_OK(Exist("/"));
  ASSERT_OK(Exist("//"));
  ASSERT_OK(Exist("///"));
}

TEST(FilesystemTest, Files) {
  Stat tmp;
  ASSERT_OK(fs_->OpenFilesystem(fsloc_));
  ASSERT_OK(Creat("/1"));
  ASSERT_CONFLICT(Creat("/1"));
  ASSERT_OK(Exist("/1"));
  ASSERT_OK(Exist("//1"));
  ASSERT_OK(Exist("///1"));
  ASSERT_ERR(Exist("/1/"));
  ASSERT_ERR(Exist("//1//"));
  ASSERT_NOTFOUND(Exist("/2"));
  ASSERT_OK(Creat("/2"));
}

TEST(FilesystemTest, Dirs) {
  ASSERT_OK(fs_->OpenFilesystem(fsloc_));
  ASSERT_OK(Mkdir("/1"));
  ASSERT_CONFLICT(Mkdir("/1"));
  ASSERT_CONFLICT(Creat("/1"));
  ASSERT_OK(Exist("/1"));
  ASSERT_OK(Exist("/1/"));
  ASSERT_OK(Exist("//1"));
  ASSERT_OK(Exist("//1//"));
  ASSERT_OK(Exist("///1"));
  ASSERT_OK(Exist("///1///"));
  ASSERT_NOTFOUND(Exist("/2"));
  ASSERT_OK(Mkdir("/2"));
}

TEST(FilesystemTest, Subdirs) {
  ASSERT_OK(fs_->OpenFilesystem(fsloc_));
  ASSERT_OK(Mkdir("/1"));
  ASSERT_OK(Mkdir("/1/a"));
  ASSERT_CONFLICT(Mkdir("/1/a"));
  ASSERT_CONFLICT(Creat("/1/a"));
  ASSERT_OK(Exist("/1/a"));
  ASSERT_OK(Exist("/1/a/"));
  ASSERT_OK(Exist("//1//a"));
  ASSERT_OK(Exist("//1//a//"));
  ASSERT_OK(Exist("///1///a"));
  ASSERT_OK(Exist("///1///a///"));
  ASSERT_NOTFOUND(Exist("/1/b"));
  ASSERT_OK(Mkdir("/1/b"));
}

TEST(FilesystemTest, Resolv) {
  ASSERT_OK(fs_->OpenFilesystem(fsloc_));
  ASSERT_OK(Mkdir("/1"));
  ASSERT_OK(Mkdir("/1/2"));
  ASSERT_OK(Mkdir("/1/2/3"));
  ASSERT_OK(Mkdir("/1/2/3/4"));
  ASSERT_OK(Mkdir("/1/2/3/4/5"));
  ASSERT_OK(Creat("/1/2/3/4/5/6"));
  ASSERT_OK(Exist("/1"));
  ASSERT_OK(Exist("/1/2"));
  ASSERT_OK(Exist("/1/2/3"));
  ASSERT_OK(Exist("/1/2/3/4"));
  ASSERT_OK(Exist("/1/2/3/4/5"));
  ASSERT_ERR(Exist("/1/2/3/4/5/6/"));
  ASSERT_ERR(Exist("/2/3"));
  ASSERT_ERR(Exist("/1/2/4/5"));
  ASSERT_ERR(Exist("/1/2/3/5"));
  ASSERT_ERR(Creat("/1/2/3/4/5/6/7"));
}

TEST(FilesystemTest, Listdir1) {
  ASSERT_OK(fs_->OpenFilesystem(fsloc_));
  ASSERT_OK(Creat("/1"));
  ASSERT_OK(Mkdir("/2"));
  ASSERT_OK(Creat("/3"));
  ASSERT_OK(Mkdir("/4"));
  ASSERT_OK(Creat("/5"));
  FilesystemDir* dir;
  ASSERT_OK(fs_->Opendir(me, NULL, "/", &dir));
  std::set<std::string> set;
  Listdir(dir, &set);
  ASSERT_TRUE(set.count("1") == 1);
  ASSERT_TRUE(set.count("2") == 1);
  ASSERT_TRUE(set.count("3") == 1);
  ASSERT_TRUE(set.count("4") == 1);
  ASSERT_TRUE(set.count("5") == 1);
  ASSERT_OK(fs_->Closdir(dir));
}

TEST(FilesystemTest, Listdir2) {
  ASSERT_OK(fs_->OpenFilesystem(fsloc_));
  ASSERT_OK(Mkdir("/1"));
  ASSERT_OK(Creat("/1/a"));
  ASSERT_OK(Mkdir("/1/b"));
  ASSERT_OK(Creat("/1/c"));
  ASSERT_OK(Mkdir("/1/d"));
  ASSERT_OK(Creat("/1/e"));
  FilesystemDir* dir;
  ASSERT_OK(fs_->Opendir(me, NULL, "/1", &dir));
  std::set<std::string> set;
  Listdir(dir, &set);
  ASSERT_TRUE(set.count("a") == 1);
  ASSERT_TRUE(set.count("b") == 1);
  ASSERT_TRUE(set.count("c") == 1);
  ASSERT_TRUE(set.count("d") == 1);
  ASSERT_TRUE(set.count("e") == 1);
  ASSERT_OK(fs_->Closdir(dir));
}

}  // namespace pdlfs

int main(int argc, char** argv) {
  return ::pdlfs::test::RunAllTests(&argc, &argv);
}
