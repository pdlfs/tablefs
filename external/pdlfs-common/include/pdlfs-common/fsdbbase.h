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

#include "pdlfs-common/fstypes.h"

namespace pdlfs {

struct DirInfo;
struct DirId {
  DirId() {}  // Intentionally not initialized for performance.
#if defined(DELTAFS_PROTO)
  DirId(uint64_t dno, uint64_t ino) : dno(dno), ino(ino) {}
#endif
#if defined(DELTAFS)
  DirId(uint64_t reg, uint64_t snap, uint64_t ino)
      : reg(reg), snap(snap), ino(ino) {}
#endif
  explicit DirId(uint64_t ino);  // Direct initialization via inodes.
  // Initialization via LookupStat or Stat.
#if defined(DELTAFS_PROTO) || defined(DELTAFS) || \
    defined(INDEXFS)  // Tablefs does not use LookupStat.
  explicit DirId(const LookupStat& stat);
#endif
  explicit DirId(const Stat& stat);

  // Three-way comparison.  Returns value:
  //   <  0 iff "*this" <  "other",
  //   == 0 iff "*this" == "other",
  //   >  0 iff "*this" >  "other"
  int compare(const DirId& other) const;
  std::string DebugString() const;

  // Deltafs requires extra fields.
#if defined(DELTAFS_PROTO)
  uint64_t dno;
#endif
#if defined(DELTAFS)
  uint64_t reg;
  uint64_t snap;
#endif

  uint64_t ino;
};

inline bool operator==(const DirId& x, const DirId& y) {
#if defined(DELTAFS_PROTO)
  if (x.dno != y.dno) {
    return false;
  }
#endif
#if defined(DELTAFS)
  if (x.reg != y.reg) return false;
  if (x.snap != y.snap) return false;
#endif

  return (x.ino == y.ino);
}

inline bool operator!=(const DirId& x, const DirId& y) {
  return !(x == y);  // Reuse operator==
}

}  // namespace pdlfs
