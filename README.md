# kduexr

          __        ___      __ 
    |__/ |  \ |  | |__  \_/ |__)
    |  \ |__/ \__/ |___ / \ |  \

## Overview

_kduexr_ demonstrates the use of the [Kakadu SDK](https://kakadusoftware.com/)
to decompress and compress OpenEXR images that use the HTJ2K Compression.

_IMPORTANT_: While _kduexr_ is published under an [open-source
license](./LICENSE.txt), the Kakadu SDK is a commercial library licensed under a
restrictive license. The _kduexr_ license does not extend to the Kakadu SDK and a
separate license for Kakadu SDK must be obained.

## Overview

<./main.cpp> uses the OpenEXR Core API to:

- decode a supplied OpenEXR scanline image (`src_file`) to baseband
- encode the baseband image to an OpenEXR scanline image (`enc_file`) using HTJ2K
  Compression. Instead of using the OpenEXR default HTJ2K Compressor, a custom
  `compress_fn` based on the KDU SDK
- decodes `enc_file` using a custom `decompress_fn` based on the KDU SDK to a
  baseband image and confirm that the baseband image is identical to the
  baseband image obtained from `src_file`

## Prerequisites

* Kakadu SDK library files (version 8.0+)
* C/C++ toolchain
* CMake

## Quick start

    git clone --recurse-submodules https://github.com/sandflow/exrkdu.git
    cd exrkdu
    mkdir build
    cd build
    cmake -DKDU_LIBRARY=<path to Kakadu SDK library, e.g. libkdu_axxR.so> \
          -DKDU_AUX_LIBRARY=<path to Kakadu SDK auxilary library, e.g. libkdu_axxR.so> \
          -DKDU_INCLUDE_DIR=<path to Kakadu SDK include headers, e.g. managed/all_includes> \
          ..
    ./bin/exrkdu SPARKS_ACES_00000.exr SPARKS_ACES_00000.j2k.exr
