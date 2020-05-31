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
#include "rados_env.h"

#include "pdlfs-common/testharness.h"

#include <vector>

// Parameters for opening ceph.
namespace {
const char* FLAGS_user_name = "client.admin";
const char* FLAGS_rados_cluster_name = "ceph";
const char* FLAGS_pool_name = "test";
const char* FLAGS_conf = NULL;  // Use ceph defaults
}  // namespace

namespace pdlfs {
namespace rados {

class RadosEnvTest {
 public:
  RadosEnvTest() : working_dir_("/testdir1/testdir2") {
    bytes_ = "xyzxyzxyz";
    RadosConnMgrOptions options;
    mgr_ = new RadosConnMgr(options);
    env_ = NULL;
  }

  void Open() {
    RadosConn* conn;
    Osd* osd;
    ASSERT_OK(mgr_->OpenConn(  ///
        FLAGS_rados_cluster_name, FLAGS_user_name, FLAGS_conf,
        RadosConnOptions(), &conn));
    ASSERT_OK(mgr_->OpenOsd(conn, FLAGS_pool_name, RadosOptions(), &osd));
    env_ = mgr_->OpenEnv(osd, true, RadosEnvOptions());
    env_->CreateDir(working_dir_.c_str());
    mgr_->Release(conn);
  }

  inline std::string TEST_filename(const char* file) {
    return working_dir_ + "/" + file;
  }

  Status Delete(const std::string& fname) {  // Dir shall be mounted readwrite
    return env_->DeleteFile(fname.c_str());
  }

  bool Exists(const std::string& fname) {
    return env_->FileExists(fname.c_str());
  }

  ~RadosEnvTest() {
    env_->DeleteDir(working_dir_.c_str());
    delete env_;
    delete mgr_;
  }

  std::string bytes_;  // Test file contents
  std::string working_dir_;
  RadosConnMgr* mgr_;
  Env* env_;
};

TEST(RadosEnvTest, ReadAndWrite) {
  Open();
  std::string fname1 = TEST_filename("f1");
  std::string fname2 = TEST_filename("f2");
  ASSERT_OK(WriteStringToFile(env_, bytes_, fname1.c_str()));
  ASSERT_TRUE(Exists(fname1));
  std::string tmp;
  ASSERT_OK(ReadFileToString(env_, fname1.c_str(), &tmp));
  ASSERT_EQ(Slice(tmp), bytes_);
  ASSERT_OK(Delete(fname1));
  ASSERT_FALSE(Exists(fname2));
}

TEST(RadosEnvTest, ListDir) {
  Open();
  std::string fname1 = TEST_filename("f1");
  std::string fname2 = TEST_filename("f2");
  ASSERT_OK(WriteStringToFile(env_, bytes_, fname1.c_str()));
  ASSERT_OK(WriteStringToFile(env_, bytes_, fname2.c_str()));
  std::vector<std::string> v;
  ASSERT_OK(env_->GetChildren(working_dir_.c_str(), &v));
  ASSERT_EQ(v.size(), 2);
  ASSERT_OK(Delete(fname1));
  ASSERT_OK(Delete(fname2));
}

TEST(RadosEnvTest, MountAndUnmount) {
  Open();
  std::string fname1 = TEST_filename("f1");
  std::string fname2 = TEST_filename("f2");
  ASSERT_OK(WriteStringToFile(env_, bytes_, fname1.c_str()));
  // Test unmount and re-mount readonly
  ASSERT_OK(env_->DetachDir(working_dir_.c_str()));
  ASSERT_OK(env_->AttachDir(working_dir_.c_str()));
  ASSERT_TRUE(Exists(fname1));
  ASSERT_ERR(WriteStringToFile(env_, bytes_, fname2.c_str()));
  ASSERT_FALSE(Exists(fname2));
  // Test unmount and re-mount readwrite
  ASSERT_OK(env_->DetachDir(working_dir_.c_str()));
  ASSERT_OK(env_->CreateDir(working_dir_.c_str()));
  ASSERT_TRUE(Exists(fname1));
  ASSERT_OK(WriteStringToFile(env_, bytes_, fname2.c_str()));
  ASSERT_TRUE(Exists(fname2));
  ASSERT_OK(Delete(fname1));
  ASSERT_OK(Delete(fname2));
}

}  // namespace rados
}  // namespace pdlfs

namespace {
inline void PrintUsage() {
  fprintf(stderr, "Use --cluster, --user, --conf, and --pool to conf test.\n");
  exit(1);
}

void ParseArgs(int argc, char* argv[]) {
  for (int i = 1; i < argc; ++i) {
    ::pdlfs::Slice a = argv[i];
    if (a.starts_with("--cluster=")) {
      FLAGS_rados_cluster_name = argv[i] + strlen("--cluster=");
    } else if (a.starts_with("--user=")) {
      FLAGS_user_name = argv[i] + strlen("--user=");
    } else if (a.starts_with("--conf=")) {
      FLAGS_conf = argv[i] + strlen("--conf=");
    } else if (a.starts_with("--pool=")) {
      FLAGS_pool_name = argv[i] + strlen("--pool=");
    } else {
      PrintUsage();
    }
  }

  printf("Cluster name: %s\n", FLAGS_rados_cluster_name);
  printf("User name: %s\n", FLAGS_user_name);
  printf("Storage pool: %s\n", FLAGS_pool_name);
  printf("Conf: %s\n", FLAGS_conf);
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc > 1) {
    ParseArgs(argc, argv);
    return ::pdlfs::test::RunAllTests(&argc, &argv);
  } else {
    return 0;
  }
}
