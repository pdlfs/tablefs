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
#include "posix_rpc_udp.h"

#include "pdlfs-common/mutexlock.h"

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

namespace pdlfs {

PosixUDPServer::PosixUDPServer(rpc::If* srv, size_t max_msgsz)
    : max_msgsz_(max_msgsz), srv_(srv) {}

Status PosixUDPServer::OpenAndBind(const std::string& uri) {
  MutexLock ml(&mutex_);
  if (fd_ != -1) {
    return Status::AssertionFailed("Socket already opened");
  }
  Status status = addr_->ResolvUri(uri);
  if (!status.ok()) {
    return status;
  }

  // Try opening the server. If we fail we will clean up so that we can try
  // again later.
  fd_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd_ == -1) {
    status = Status::IOError(strerror(errno));
  } else {
    int rv = bind(fd_, reinterpret_cast<struct sockaddr*>(addr_->rep()),
                  sizeof(struct sockaddr_in));
    if (rv == -1) {
      status = Status::IOError(strerror(errno));
      close(fd_);
      fd_ = -1;
    }
  }

  if (status.ok()) {
    // Fetch the port that we have just bound to in case we have decided to have
    // the OS choose the port
    socklen_t tmp = sizeof(struct sockaddr_in);
    getsockname(fd_, reinterpret_cast<struct sockaddr*>(actual_addr_->rep()),
                &tmp);
  }

  return status;
}

Status PosixUDPServer::BGLoop(int myid) {
  struct pollfd po;
  po.events = POLLIN;
  po.fd = fd_;
  CallState* const call = static_cast<CallState*>(
      malloc(sizeof(struct CallState) - 1 + max_msgsz_));

  int err = 0;
  while (!err && !shutting_down_.Acquire_Load()) {
    call->addrlen = sizeof(call->addr);
    // Try performing a quick non-blocking receive from peers before sinking
    // into poll.
    ssize_t rv = recvfrom(fd_, call->msg, max_msgsz_, MSG_DONTWAIT,
                          reinterpret_cast<struct sockaddr*>(&call->addr),
                          &call->addrlen);
    if (rv > 0) {
      call->msgsz = rv;
      HandleIncomingCall(call);
      continue;
    } else if (rv == 0) {  // Empty message
      continue;
    } else if (errno == EWOULDBLOCK) {
      rv = poll(&po, 1, 200);
    }

    // Either poll() or recvfrom() may have returned error
    if (rv == -1) {
      err = errno;
    }
  }

  free(call);

  Status status;
  if (err) {
    status = Status::IOError(strerror(err));
  }
  return status;
}

void PosixUDPServer::HandleIncomingCall(CallState* const call) {
  rpc::If::Message in, out;
  in.contents = Slice(call->msg, call->msgsz);
  srv_->Call(in, out);
  ssize_t nbytes =
      sendto(fd_, out.contents.data(), out.contents.size(), 0,
             reinterpret_cast<struct sockaddr*>(&call->addr), call->addrlen);
  if (nbytes != out.contents.size()) {
    //
  }
}

std::string PosixUDPServer::GetUri() {
  return std::string("udp://") + GetBaseUri();
}

PosixUDPCli::PosixUDPCli(uint64_t timeout, size_t max_msgsz)
    : rpc_timeout_(timeout), max_msgsz_(max_msgsz), fd_(-1) {}

void PosixUDPCli::Open(const std::string& uri) {
  PosixSocketAddr addr;
  status_ = addr.ResolvUri(uri);
  if (!status_.ok()) {
    return;
  }
  fd_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd_ == -1) {
    status_ = Status::IOError(strerror(errno));
    return;
  }
  int rv = connect(fd_, reinterpret_cast<struct sockaddr*>(addr.rep()),
                   sizeof(struct sockaddr_in));
  if (rv == -1) {
    status_ = Status::IOError(strerror(errno));
    close(fd_);
    fd_ = -1;
  }
}

// We do a synchronous send, followed by one or more non-blocking receives
// so that we can easily check timeouts without waiting for the data
// indefinitely. We use a timed poll to check data availability.
Status PosixUDPCli::Call(Message& in, Message& out) RPCNOEXCEPT {
  if (!status_.ok()) {
    return status_;
  }
  Status status;
  ssize_t rv = send(fd_, in.contents.data(), in.contents.size(), 0);
  if (rv != in.contents.size()) {
    status = Status::IOError(strerror(errno));
    return status;
  }
  const uint64_t start = CurrentMicros();
  std::string& buf = out.extra_buf;
  buf.reserve(max_msgsz_);
  buf.resize(1);
  struct pollfd po;
  memset(&po, 0, sizeof(struct pollfd));
  po.events = POLLIN;
  po.fd = fd_;
  while (true) {
    rv = recv(fd_, &buf[0], max_msgsz_, MSG_DONTWAIT);
    if (rv > 0) {
      out.contents = Slice(&buf[0], rv);
      break;
    } else if (rv == 0) {  // Empty reply
      out.contents = Slice();
      break;
    } else if (errno == EWOULDBLOCK) {
      // We wait for 0.2 second and therefore timeouts are only checked
      // roughly every that amount of time.
      rv = poll(&po, 1, 200);
    }

    // Either poll() or recv() may have returned an error
    if (rv == -1) {
      status = Status::IOError(strerror(errno));
      break;
    } else if (rv == 1) {
      continue;
    } else if (CurrentMicros() - start >= rpc_timeout_) {
      status = Status::Disconnected("timeout");
      break;
    }
  }

  return status;
}

PosixUDPCli::~PosixUDPCli() {
  if (fd_ != -1) {
    close(fd_);
  }
}

}  // namespace pdlfs
