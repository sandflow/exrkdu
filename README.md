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
    cmake -DKDU_LIBRARY=<path to Kakadu SDK library, e.g. libkdu_vxxR.so> \
          -DKDU_AUX_LIBRARY=<path to Kakadu SDK auxilary library, e.g. libkdu_axxR.so> \
          -DKDU_INCLUDE_DIR=<path to Kakadu SDK include headers, e.g. managed/all_includes> \
          ..
    ./bin/exrkdu SPARKS_ACES_00000.exr SPARKS_ACES_00000.j2k.exr

## Special instructions for MacOS

There are different ways to configure dynamic libraries and locations on MacOS, here is one example:

Build Kakadu SDK like normal, i.e.

    cd ~/software/kdu-sdk/v8_5-01908E/make
    make -f Makefile-Mac-arm-64-gcc all_but_jni

Copy  libkdu_axxR.so and libkdu_vxxR.so into ~/lib

    cd ~
    mkdir lib
    cd ~/software/kdu-sdk/v8_5-01908E/lib/Mac-arm-64-gcc
    cp *.so ~/lib

Add the following lines to the session profile file `~/.zprofile`, you can edit that file by typing `nano ~/.zprofile`

    DYLD_LIBRARY_PATH=~/lib
    export DYLD_LIBRARY_PATH

Navigate to build directory:

    cd ~/software/exrkdu/build

Run cmake to configure OpenEXR with Kakadu

    cmake .. \
          -DKDU_INCLUDE_DIR=~/software/kdu-sdk/v8_5-01908E/managed/all_includes \
          -DKDU_LIBRARY=~/lib/libkdu_v85R.so \
          -DKDU_AUX_LIBRARY=~/lib/libkdu_a85R.so

Run make to build OpenEXR including exrkdu

    make

Navigate to bin directory

    cd ~/software/exrkdu/build

Run exrkdu - encode test image [Tree.exr](https://openexr.com/en/latest/test_images/ScanLines/Tree.html) with Kakadu HTJ2K and decode with Kakadu HTJ2K

    ./exrkdu --ipath ~/Downloads/Tree.exr --epath ~/Downloads/Tree.kdu.exr

Run exrkdu - encode test image [Tree.exr](https://openexr.com/en/latest/test_images/ScanLines/Tree.html) with Kakadu HTJ2K and decode with default HTJ2K decoder (OpenJPH)

    ./exrkdu --ipath ~/Downloads/Tree.exr --epath ~/Downloads/Tree.kdu.exr -d

