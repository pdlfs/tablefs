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
#include "fs.h"
#include "fsdb.h"
#include "port.h"

#include "pdlfs-common/testharness.h"

#include <sys/stat.h>
#include <set>
#include <string>

namespace pdlfs {

class FilesystemTest {
 public:
  FilesystemTest() {
    fsloc_ = test::TmpDir() + "/filesystem_test";
    DestroyDb(fsloc_);
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

  Status Exist(const char* path) {
    Stat ignored;
    return fs_->Lstat(me, path, &ignored, &stats_);
  }

  Status Creat(const char* path) { return fs_->Creat(me, path, 0660, &stats_); }

  Status Mkdir(const char* path) { return fs_->Mkdir(me, path, 0770, &stats_); }

  ~FilesystemTest() {
    if (fs_) {
      delete fs_;
    }
  }

  User me;
  std::string fsloc_;
  FilesystemDbStats stats_;
  FilesystemOptions options_;
  Filesystem* fs_;
};

TEST(FilesystemTest, OpenAndClose) {
  ASSERT_OK(fs_->OpenFilesystem(fsloc_));
  ASSERT_OK(Exist("/"));
  ASSERT_OK(Exist("//"));
  ASSERT_OK(Exist("///"));
  ASSERT_OK(Creat("/1"));
  ASSERT_EQ(fs_->TEST_GetCurrentInoseq(), 2);
  delete fs_;
  fs_ = new Filesystem(options_);
  ASSERT_OK(fs_->OpenFilesystem(fsloc_));
  ASSERT_EQ(fs_->TEST_GetCurrentInoseq(), 2);
  ASSERT_OK(Exist("/"));
  ASSERT_OK(Exist("//"));
  ASSERT_OK(Exist("///"));
  ASSERT_OK(Exist("/1"));
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
  ASSERT_OK(fs_->Opendir(me, "/", &dir, NULL));
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
  ASSERT_OK(fs_->Opendir(me, "/1", &dir, NULL));
  std::set<std::string> set;
  Listdir(dir, &set);
  ASSERT_TRUE(set.count("a") == 1);
  ASSERT_TRUE(set.count("b") == 1);
  ASSERT_TRUE(set.count("c") == 1);
  ASSERT_TRUE(set.count("d") == 1);
  ASSERT_TRUE(set.count("e") == 1);
  ASSERT_OK(fs_->Closdir(dir));
}

namespace {
inline int GetIntegerOptionFromEnv(const char* key, int def) {
  const char* const env = getenv(key);
  if (env && env[0]) {
    return atoi(env);
  } else {
    return def;
  }
}

int GetOpt(const char* key, int def) {
  int opt = GetIntegerOptionFromEnv(key, def);
  fprintf(stderr, "%s=%d\n", key, opt);
  return opt;
}
}  // namespace

/*
 * Insert directories and files into an empty filesystem image. The directories
 * and files are inserted according to a fan-out factor (2 in the example
 * below), a tree depth (2 in the example), and a files-per-leaf-dir number (3
 * in the example), as illustrated below.
 *
 * Depth 0  ------------>     ROOT
 *                          /      \
 * Depth 1  ------>   Dir 1         Dir 2
 *                  /     \        /     \
 * Depth 2  -> Dir 1   Dir 2    Dir 1    Dir 2   <-- Leaf directories
 *             / | \   / | \    / | \    / | \
 *           F1 F2 F3 F1 F2 F3 F1 F2 F3 F1 F2 F3
 */
class FilesystemLoader {
 public:
  explicit FilesystemLoader(const std::string& fsloc) : fsloc_(fsloc) {
    files_per_leaddir_ = GetOpt("FILES_PER_LEAFDIR", 3);
    tree_depth_ = GetOpt("TREE_DEPTH", 2);
    me_.gid = GetOpt("USER_GROUP_ID", 1);
    me_.uid = GetOpt("USER_ID", 1);
    f_ = GetOpt("FAN_OUT", 2);
  }

  void Doit(Filesystem* fs, std::string* path, int depth) {
    const size_t prefix_len = path->size();
    if (depth == tree_depth_) {  // This is the leaf level of directories
      for (int i = 0; i < files_per_leaddir_; i++) {
        path->push_back('a' + i);
        ASSERT_OK(fs->Creat(me_, path->c_str(), 0644, NULL));
        fprintf(stderr, "%s\n", path->c_str());
        path->resize(prefix_len);
      }
    } else {
      for (int i = 0; i < f_; i++) {
        path->push_back('A' + i);
        ASSERT_OK(fs->Mkdir(me_, path->c_str(), 0755, NULL));
        fprintf(stderr, "%s\n", path->c_str());
        path->push_back('/');  // Add path delimiter
        Doit(fs, path, depth + 1);
        path->resize(prefix_len);
      }
    }
  }

  void Run() {
    DestroyDb(fsloc_);
    Filesystem* const fs = new Filesystem(options_);
    ASSERT_OK(fs->OpenFilesystem(fsloc_));
    std::string path("/");
    Doit(fs, &path, 0);
    delete fs;
  }

  User me_;
  std::string fsloc_;
  FilesystemOptions options_;
  int files_per_leaddir_;
  int tree_depth_;
  int f_;
};

/*
 * List contents populated by FilesystemLoader.
 */
class FilesystemLister {
 public:
  explicit FilesystemLister(const std::string& fsloc) : fsloc_(fsloc) {
    me_.gid = GetOpt("USER_GROUP_ID", 1);
    me_.uid = GetOpt("USER_ID", 1);
    if (!GetOpt("DISABLE_READONLY", 0)) {
      options_.rdonly = true;
    }
  }

  void Doit(Filesystem* fs, std::string* path) {
    FilesystemDir* dir;
    const size_t prefix_len = path->size();
    ASSERT_OK(fs->Opendir(me_, path->c_str(), &dir, NULL));
    std::string name;
    Stat stat;
    for (;;) {
      Status status = fs->Readdir(dir, &stat, &name);
      if (status.IsNotFound()) {
        break;
      }

      ASSERT_OK(status);

      if (S_ISREG(stat.FileMode())) {
        fprintf(stderr, "%s%s\n", path->c_str(), name.c_str());
      } else {
        path->append(name);
        fprintf(stderr, "%s\n", path->c_str());
        path->push_back('/');
        Doit(fs, path);
        path->resize(prefix_len);
      }
    }

    fs->Closdir(dir);
  }

  void Run() {
    Filesystem* const fs = new Filesystem(options_);
    ASSERT_OK(fs->OpenFilesystem(fsloc_));
    std::string path("/");
    Doit(fs, &path);
    delete fs;
  }

  FilesystemOptions options_;
  std::string fsloc_;
  User me_;
};

}  // namespace pdlfs

#include "pdlfs-common/pdlfs_config.h"
#if defined(PDLFS_GFLAGS)
#include <gflags/gflags.h>
#endif
#if defined(PDLFS_GLOG)
#include <glog/logging.h>
#endif

namespace {
void BM_Usage() {
  fprintf(stderr, "Use --prog=[loader,lister] <fsloc> to run programs.\n");
  fprintf(stderr, "\n");
  exit(EXIT_FAILURE);
}

void BM_Main(int* argc, char*** argv) {
#if defined(PDLFS_GFLAGS)
  google::ParseCommandLineFlags(argc, argv, true);
#endif
#if defined(PDLFS_GLOG)
  google::InitGoogleLogging((*argv)[0]);
  google::InstallFailureSignalHandler();
#endif
  pdlfs::Slice prog_name;
  if (*argc > 2) {
    prog_name = pdlfs::Slice((*argv)[*argc - 2]);
  } else {
    BM_Usage();
  }
  const char* fsloc = (*argv)[*argc - 1];
  if (prog_name == "--prog=loader") {
    pdlfs::FilesystemLoader prog(fsloc);
    prog.Run();
  } else if (prog_name == "--prog=lister") {
    pdlfs::FilesystemLister prog(fsloc);
    prog.Run();
  } else {
    BM_Usage();
  }
}
}  // namespace

int main(int argc, char** argv) {
  pdlfs::Slice token1, token2;
  if (argc > 2) {
    token2 = pdlfs::Slice(argv[argc - 2]);
  }
  if (argc > 1) {
    token1 = pdlfs::Slice(argv[argc - 1]);
  }
  if (!token1.starts_with("--prog") && !token2.starts_with("--prog")) {
    return pdlfs::test::RunAllTests(&argc, &argv);
  } else {
    BM_Main(&argc, &argv);
    return 0;
  }
}
