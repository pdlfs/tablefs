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

#if defined(TABLEFS_PORT_KVRANGE) || defined(TABLEFS_PORT_KVRANGEDB)
#include "port/port_kvrange.h"
#elif defined(TABLEFS_PORT_LEVELDB)
#include "port/port_leveldb.h"
#else
#include "port/port_default.h"
#endif
