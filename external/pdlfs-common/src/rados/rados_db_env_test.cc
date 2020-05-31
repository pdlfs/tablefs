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
#include "rados_db_env.h"

#include "pdlfs-common/leveldb/db.h"
#include "pdlfs-common/leveldb/options.h"
#include "pdlfs-common/testharness.h"

#include <algorithm>
#include <stdio.h>
#include <string.h>
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
  RadosEnvTest() : working_dir_(test::TmpDir() + "/rados_env_test") {
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
    env_ = mgr_->CreateDbEnvWrapper(env_, true, RadosDbEnvOptions());
    env_->CreateDir(working_dir_.c_str());
    mgr_->Release(conn);
  }

  ~RadosEnvTest() {
    env_->DeleteDir(working_dir_.c_str());
    delete env_;
    delete mgr_;
  }

  std::string working_dir_;
  RadosConnMgr* mgr_;
  Env* env_;
};

TEST(RadosEnvTest, FileLock) {
  Open();
  FileLock* lock;
  std::string fname = LockFileName(working_dir_);
  ASSERT_OK(env_->LockFile(fname.c_str(), &lock));
  ASSERT_OK(env_->UnlockFile(lock));
  ASSERT_OK(env_->DeleteFile(fname.c_str()));
}

TEST(RadosEnvTest, SetCurrentFile) {
  Open();
  ASSERT_OK(SetCurrentFile(env_, working_dir_, 1));
  std::string fname = CurrentFileName(working_dir_);
  ASSERT_TRUE(env_->FileExists(fname.c_str()));
  ASSERT_OK(env_->DeleteFile(fname.c_str()));
}

TEST(RadosEnvTest, ListDbFiles) {
  Open();
  std::vector<std::string> fnames;
  fnames.push_back(DescriptorFileName(working_dir_, 1));
  fnames.push_back(LogFileName(working_dir_, 2));
  fnames.push_back(TableFileName(working_dir_, 3));
  fnames.push_back(SSTTableFileName(working_dir_, 4));
  fnames.push_back(TempFileName(working_dir_, 5));
  fnames.push_back(InfoLogFileName(working_dir_));
  fnames.push_back(OldInfoLogFileName(working_dir_));
  for (size_t i = 0; i < fnames.size(); i++) {
    ASSERT_OK(WriteStringToFile(env_, "xyz", fnames[i].c_str()));
  }
  std::vector<std::string> r;
  ASSERT_OK(env_->GetChildren(working_dir_.c_str(), &r));
  for (size_t i = 0; i < fnames.size(); i++) {
    ASSERT_TRUE(std::find(r.begin(), r.end(),
                          fnames[i].substr(working_dir_.size() + 1)) !=
                r.end());
    ASSERT_OK(env_->DeleteFile(fnames[i].c_str()));
  }
}

TEST(RadosEnvTest, Db) {
  Open();
  DBOptions options;
  options.create_if_missing = true;
  options.env = env_;
  DB* db;
  ASSERT_OK(DB::Open(options, working_dir_, &db));
  WriteOptions wo;
  ASSERT_OK(db->Put(wo, "k1", "v1"));
  FlushOptions fo;
  ASSERT_OK(db->FlushMemTable(fo));
  db->CompactRange(NULL, NULL);
  ASSERT_OK(db->Put(wo, "k2", "v2"));
  delete db;
  options.error_if_exists = false;
  ASSERT_OK(DB::Open(options, working_dir_, &db));
  delete db;
  DestroyDB(working_dir_, options);
}

TEST(RadosEnvTest, IoSimplifiedDb) {
  Open();
  DBOptions options;
  options.info_log = Logger::Default();
  options.rotating_manifest = true;
  options.skip_lock_file = true;
  options.create_if_missing = true;
  options.env = dynamic_cast<RadosDbEnvWrapper*>(env_)->TEST_GetRadosEnv();
  DB* db;
  ASSERT_OK(DB::Open(options, working_dir_, &db));
  WriteOptions wo;
  ASSERT_OK(db->Put(wo, "k1", "v1"));
  FlushOptions fo;
  ASSERT_OK(db->FlushMemTable(fo));
  db->CompactRange(NULL, NULL);
  ASSERT_OK(db->Put(wo, "k2", "v2"));
  delete db;
  options.error_if_exists = false;
  ASSERT_OK(DB::Open(options, working_dir_, &db));
  delete db;
  DestroyDB(working_dir_, options);
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
