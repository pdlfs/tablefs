**Fast and efficient filesystem metadata through LSM-Trees.**

[![CI](https://github.com/pdlfs/tablefs/actions/workflows/ci.yml/badge.svg)](https://github.com/pdlfs/tablefs/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-New%20BSD-blue.svg)](LICENSE.txt)

TableFS by DeltaFS
=======

This is the DeltaFS re-implementation of the [TableFS](https://www.usenix.org/system/files/conference/atc13/atc13-ren.pdf) paper published by Kai and Garth at [USENIX ATC 2013](https://www.usenix.org/conference/atc13). THIS IS NOT THE ORIGINAL TABLEFS CODE. The original TableFS code is written by Kai and is available [here](https://www.cs.cmu.edu/~kair/code/tablefs-0.3.tar.gz). Internally, DeltaFS uses this re-implementation of TableFS to evaluate different local KV-store design options and to assist DeltaFS development. This re-implementation reuses some of the DeltaFS code. Again, this is not the original TableFS code.

```
XXXXXXXXX
XX      XX                 XX                  XXXXXXXXXXX
XX       XX                XX                  XX
XX        XX               XX                  XX
XX         XX              XX   XX             XX
XX          XX             XX   XX             XXXXXXXXX
XX           XX  XXXXXXX   XX XXXXXXXXXXXXXXX  XX         XX
XX          XX  XX     XX  XX   XX       XX XX XX      XX
XX         XX  XX       XX XX   XX      XX  XX XX    XX
XX        XX   XXXXXXXXXX  XX   XX     XX   XX XX    XXXXXXXX
XX       XX    XX          XX   XX    XX    XX XX           XX
XX      XX      XX      XX XX   XX X    XX  XX XX         XX
XXXXXXXXX        XXXXXXX   XX    XX        XX  XX      XX
```

DeltaFS was developed, in part, under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. Please see the accompanying [LICENSE.txt](LICENSE.txt) for further information.
