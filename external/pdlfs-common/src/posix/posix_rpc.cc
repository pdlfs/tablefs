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
#include "posix_rpc.h"

#include "posix_rpc_tcp.h"
#include "posix_rpc_udp.h"

#include "pdlfs-common/mutexlock.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

namespace pdlfs {

std::string PosixSocketAddr::GetUri() const {
  char host[INET_ADDRSTRLEN];
  char tmp[50];
  snprintf(tmp, sizeof(tmp), "%s:%d",
           inet_ntop(AF_INET, &addr_.sin_addr, host, sizeof(host)),
           ntohs(addr_.sin_port));
  return tmp;
}

void PosixSocketAddr::Reset() {
  memset(&addr_, 0, sizeof(struct sockaddr_in));
  addr_.sin_family = AF_INET;
}

Status PosixSocketAddr::ResolvUri(const std::string& uri) {
  std::string host, port;
  // E.g.: uri = "ignored://127.0.0.1:22222", "127.0.0.1", ":22222"
  //                     |  |        |         |            |
  //                     |  |        |         |            |
  //                     a  b        c         b           b,c
  size_t a = uri.find("://");  // Ignore protocol definition
  size_t b = (a == std::string::npos) ? 0 : a + 3;
  size_t c = uri.find(':', b);
  if (c != std::string::npos) {
    host = uri.substr(b, c - b);
    port = uri.substr(c + 1);
  } else {
    host = uri.substr(b);
  }

  Status status;
  if (host.empty()) {
    addr_.sin_addr.s_addr = INADDR_ANY;
  } else {
    int h1, h2, h3, h4;
    char junk;
    const bool is_numeric =
        sscanf(host.c_str(), "%d.%d.%d.%d%c", &h1, &h2, &h3, &h4, &junk) == 4;
    status = Resolv(host.c_str(), is_numeric);
  }
  if (status.ok()) {
    SetPort(port.c_str());
  }
  return status;
}

Status PosixSocketAddr::Resolv(const char* host, bool is_numeric) {
  struct addrinfo *ai, hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;
  if (is_numeric) {
    hints.ai_flags = AI_NUMERICHOST;
  }
  int rv = getaddrinfo(host, NULL, &hints, &ai);
  if (rv != 0) {
    return Status::IOError("getaddrinfo", gai_strerror(rv));
  }
  const struct sockaddr_in* const in =
      reinterpret_cast<struct sockaddr_in*>(ai->ai_addr);
  addr_.sin_addr = in->sin_addr;
  freeaddrinfo(ai);
  return Status::OK();
}

PosixSocketServer::PosixSocketServer()
    : shutting_down_(NULL),
      bg_cv_(&mutex_),
      bg_n_(0),
      bg_threads_(0),
      bg_id_(0),
      actual_addr_(new PosixSocketAddr),
      addr_(new PosixSocketAddr),
      fd_(-1) {}

void PosixSocketServer::BGLoopWrapper(void* arg) {
  PosixSocketServer* const r = reinterpret_cast<PosixSocketServer*>(arg);
  r->BGCall();
}

void PosixSocketServer::BGCall() {
  MutexLock ml(&mutex_);
  int myid = bg_id_++;
  ++bg_threads_;
  if (bg_threads_ == bg_n_) {
    bg_cv_.SignalAll();
  }
  mutex_.Unlock();
  Status s = BGLoop(myid);  // This transfers ctrl to rpc subclass impl
  mutex_.Lock();
  if (!s.ok() && bg_status_.ok()) {
    bg_status_ = s;
  }
  assert(bg_threads_ > 0);
  --bg_threads_;
  if (!bg_threads_) {
    bg_cv_.SignalAll();
  }
}

std::string PosixSocketServer::GetBaseUri() {
  MutexLock ml(&mutex_);
  if (fd_ != -1) return actual_addr_->GetUri();
  return addr_->GetUri();
}

Status PosixSocketServer::status() {
  MutexLock ml(&mutex_);
  return bg_status_;
}

Status PosixSocketServer::BGStart(Env* const env, int num_threads) {
  MutexLock ml(&mutex_);
  bg_n_ += num_threads;
  for (int i = 0; i < num_threads; i++) {
    env->StartThread(BGLoopWrapper, this);
  }
  while (bg_threads_ < bg_n_) {
    bg_cv_.Wait();
  }
  // All background threads have started and they may have already
  // encountered errors and have exited so now is a good time we report
  // any errors back to the user.
  return bg_status_;
}

Status PosixSocketServer::BGStop() {
  MutexLock ml(&mutex_);
  shutting_down_.Release_Store(this);
  while (bg_threads_) {
    bg_cv_.Wait();
  }
  return bg_status_;  // Report background error status
}

PosixSocketServer::~PosixSocketServer() {
  BGStop();  // Stop background progressing
  delete actual_addr_;
  delete addr_;
  if (fd_ != -1) {
    close(fd_);
  }
}

PosixRPC::PosixRPC(const RPCOptions& options)
    : srv_(NULL), options_(options), tcp_(0) {
  tcp_ = Slice(options_.uri).starts_with("tcp://");
  if (options_.mode == rpc::kServerClient) {
    if (tcp_) {
      srv_ = new PosixTCPServer(options_.fs, options_.rpc_timeout);
    } else {
      srv_ = new PosixUDPServer(options_.fs);
    }
  }
}

Status PosixRPC::Start() {
  Status status;
  if (srv_) {
    status = srv_->OpenAndBind(options_.uri);
    if (status.ok()) {
      // BGStart() will wait until all threads are up
      status = srv_->BGStart(options_.env, options_.num_rpc_threads);
    }
  }
  return status;
}

Status PosixRPC::Stop() {
  if (srv_) return srv_->BGStop();
  return Status::OK();
}

std::string PosixRPC::GetUri() {
  if (srv_) return srv_->GetUri();
  return "-1:-1";
}

Status PosixRPC::status() {
  if (srv_) return srv_->status();
  return Status::OK();
}

rpc::If* PosixRPC::OpenStubFor(const std::string& uri) {
  if (!tcp_) {
    PosixUDPCli* const cli = new PosixUDPCli(options_.rpc_timeout);
    cli->Open(uri);
    return cli;
  } else {
    PosixTCPCli* const cli = new PosixTCPCli(options_.rpc_timeout);
    cli->SetTarget(uri);
    return cli;
  }
}

}  // namespace pdlfs
