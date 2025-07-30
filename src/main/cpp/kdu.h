/*
** SPDX-License-Identifier: BSD-3-Clause
** Copyright Contributors to the OpenEXR Project.
*/

#ifndef KDU_H
#define KDU_H

#include "openexr_decode.h"
#include "openexr_encode.h"

extern "C" exr_result_t
kdu_decompress (exr_decode_pipeline_t* decode);

#endif