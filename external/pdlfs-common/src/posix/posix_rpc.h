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
#pragma once

#include "pdlfs-common/port.h"
#include "pdlfs-common/rpc.h"

#include <netinet/in.h>
#include <stdlib.h>

namespace pdlfs {

// A simple wrapper class over struct sockaddr_in.
class PosixSocketAddr {
 public:
  PosixSocketAddr() { Reset(); }
  void Reset();
  // The returned uri may not be used for clients to connect to the address.
  // While both the address and the port will be numeric, the address may be
  // "0.0.0.0" and the port may be "0".
  std::string GetUri() const;
  Status ResolvUri(const std::string& uri);
  const struct sockaddr_in* rep() const { return &addr_; }
  struct sockaddr_in* rep() {
    return &addr_;
  }

 private:
  void SetPort(const char* p) {
    int port = -1;
    if (p && p[0]) port = atoi(p);
    if (port < 0) {
      // Have the OS pick up a port for us
      port = 0;
    }
    addr_.sin_port = htons(port);
  }
  // Translate a human-readable host name into a binary internet address to
  // which we can bind or connect. Return OK on success, or a non-OK status on
  // errors.
  Status Resolv(const char* host, bool is_numeric);
  struct sockaddr_in addr_;

  // Copyable
};

// Base RPC impl providing infrastructure for background progressing. To be
// extended by subclasses.
class PosixSocketServer {
 public:
  explicit PosixSocketServer(const RPCOptions& options);
  virtual ~PosixSocketServer();

  virtual std::string GetUri() = 0;
  virtual Status OpenAndBind(const std::string& uri) = 0;
  Status BGStart(Env* env, int num_threads);
  Status BGStop();

  // Return base uri of the server. A base uri does not contain protocol
  // information.
  std::string GetBaseUri();
  Status status();

 protected:
  // No copying allowed
  void operator=(const PosixSocketServer&);
  PosixSocketServer(const PosixSocketServer& other);
  // BGLoopWrapper() calls BGCall(), which calls BGLoop()
  static void BGLoopWrapper(void* arg);
  void BGCall();
  virtual Status BGLoop(int myid) = 0;  // To be implemented by subclasses...

  const RPCOptions& options_;  // For options_.info_log and options_.fs
  port::Mutex mutex_;
  // bg state below is protected by mutex_
  port::AtomicPointer shutting_down_;
  port::CondVar bg_cv_;
  int bg_n_;        // Total number of background threads to run
  int bg_threads_;  // Number of threads currently running
  int bg_id_;
  Status bg_status_;
  PosixSocketAddr* actual_addr_;
  PosixSocketAddr* addr_;
  int fd_;
};

// Posix RPC impl wrapper.
class PosixRPC : public RPC {
 public:
  explicit PosixRPC(const RPCOptions& options);
  virtual ~PosixRPC() { delete srv_; }

  virtual rpc::If* OpenStubFor(const std::string& uri);
  virtual Status Start();  // Open server the start background progressing
  virtual Status Stop();

  virtual std::string GetUri();
  virtual Status status();

 private:
  // No copying allowed
  void operator=(const PosixRPC& other);
  PosixRPC(const PosixRPC&);
  PosixSocketServer* srv_;  // NULL for client only mode
  RPCOptions options_;
  int tcp_;  // O for UDP, non-0 for TCP
};

}  // namespace pdlfs
