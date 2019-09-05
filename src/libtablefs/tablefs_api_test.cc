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

#include "tablefs/tablefs_api.h"

#include "port.h"

#include "pdlfs-common/random.h"
#include "pdlfs-common/testharness.h"

#include <map>
#include <set>

namespace pdlfs {

class FilesystemAPI {
 public:
  FilesystemAPI() {
    fsloc_ = test::TmpDir() + "/filesystem_api_test";
    DestroyDb(fsloc_, MDB::DbOpts());
    fs_ = tablefs_newfshdl();
  }

  void Listdir(tablefs_dir_t* dh, std::map<std::string, uint32_t>* map) {
    struct dirent* ent;
    while (true) {
      ent = tablefs_readdir(dh);
      if (ent) {
        (*map)[ent->d_name] = ent->d_type;
      } else {
        break;
      }
    }
  }

  uint32_t Fmode(const char* path) {
    struct stat buf;
    int r = tablefs_lstat(fs_, path, &buf);
    ASSERT_TRUE(r == 0);
    return buf.st_mode;
  }

  uint64_t Fid(const char* path) {
    struct stat buf;
    int r = tablefs_lstat(fs_, path, &buf);
    ASSERT_TRUE(r == 0);
    return buf.st_ino;
  }

  void Creat(const char* path) {
    int r = tablefs_mkfil(fs_, path, 0660);
    ASSERT_TRUE(r == 0);
  }

  void Mkdir(const char* path) {
    int r = tablefs_mkdir(fs_, path, 0770);
    ASSERT_TRUE(r == 0);
  }

  ~FilesystemAPI() {
    if (fs_) {
      tablefs_closefs(fs_);
    }
  }

  std::string fsloc_;
  tablefs_t* fs_;
};

TEST(FilesystemAPI, OpenFs) {
  int r = tablefs_openfs(fs_, fsloc_.c_str());
  ASSERT_TRUE(r == 0);
}

TEST(FilesystemAPI, Fmodes) {
  int r = tablefs_openfs(fs_, fsloc_.c_str());
  ASSERT_TRUE(r == 0);
  Mkdir("/1");
  Creat("/2");
  Mkdir("/3");
  Creat("/4");
  Mkdir("/5");
  ASSERT_TRUE(S_ISDIR(Fmode("/1")));
  ASSERT_TRUE(S_ISREG(Fmode("/2")));
  ASSERT_TRUE(S_ISDIR(Fmode("/3")));
  ASSERT_TRUE(S_ISREG(Fmode("/4")));
  ASSERT_TRUE(S_ISDIR(Fmode("/5")));
}

TEST(FilesystemAPI, Fids) {
  int r = tablefs_openfs(fs_, fsloc_.c_str());
  ASSERT_TRUE(r == 0);
  Mkdir("/1");
  Creat("/2");
  Mkdir("/3");
  Creat("/4");
  Mkdir("/5");
  std::set<uint64_t> ids;
  ASSERT_TRUE(ids.insert(Fid("/1")).second);
  ASSERT_TRUE(ids.insert(Fid("/2")).second);
  ASSERT_TRUE(ids.insert(Fid("/3")).second);
  ASSERT_TRUE(ids.insert(Fid("/4")).second);
  ASSERT_TRUE(ids.insert(Fid("/5")).second);
  ASSERT_TRUE(ids.size() == 5);
}

TEST(FilesystemAPI, Listdirs1) {
  int r = tablefs_openfs(fs_, fsloc_.c_str());
  ASSERT_TRUE(r == 0);
  Mkdir("/1");
  Creat("/2");
  Mkdir("/3");
  Creat("/4");
  Mkdir("/5");
  tablefs_dir_t* dir;
  std::map<std::string, uint32_t> map;
  dir = tablefs_opendir(fs_, "/");
  Listdir(dir, &map);
  ASSERT_TRUE(map.size() == 5);
  ASSERT_TRUE(map["1"] == DT_DIR);
  ASSERT_TRUE(map["2"] == DT_REG);
  ASSERT_TRUE(map["3"] == DT_DIR);
  ASSERT_TRUE(map["4"] == DT_REG);
  ASSERT_TRUE(map["5"] == DT_DIR);
  tablefs_closedir(dir);
}

TEST(FilesystemAPI, Listdirs2) {
  int r = tablefs_openfs(fs_, fsloc_.c_str());
  ASSERT_TRUE(r == 0);
  Mkdir("/1");
  Mkdir("/1/a");
  Creat("/1/b");
  Mkdir("/1/c");
  Creat("/1/d");
  Mkdir("/1/e");
  tablefs_dir_t* dir;
  std::map<std::string, uint32_t> map;
  dir = tablefs_opendir(fs_, "/1");
  Listdir(dir, &map);
  ASSERT_TRUE(map.size() == 5);
  ASSERT_TRUE(map["a"] == DT_DIR);
  ASSERT_TRUE(map["b"] == DT_REG);
  ASSERT_TRUE(map["c"] == DT_DIR);
  ASSERT_TRUE(map["d"] == DT_REG);
  ASSERT_TRUE(map["e"] == DT_DIR);
  tablefs_closedir(dir);
}

}  // namespace pdlfs

int main(int argc, char** argv) {
  return ::pdlfs::test::RunAllTests(&argc, &argv);
}
