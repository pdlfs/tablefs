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
    DestroyDb(fsloc_);
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
    int r = tablefs_mkfile(fs_, path, 0660);
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
