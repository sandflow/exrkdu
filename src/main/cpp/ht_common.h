/*
** SPDX-License-Identifier: BSD-3-Clause
** Copyright Contributors to the OpenEXR Project.
*/

#ifndef HT_COMMON_H
#define HT_COMMON_H

#include <vector>
#include <stdlib.h>
#include "openexr_coding.h"

struct CodestreamChannelInfo
{
    int file_index;
    size_t raster_line_offset;
};

bool make_channel_map(
    int channel_count, exr_coding_channel_info_t *channels, std::vector<CodestreamChannelInfo> &cs_to_file_ch);

size_t write_header(uint8_t *buffer, size_t max_sz, const std::vector<CodestreamChannelInfo> &map);

void read_header(void *buffer, size_t max_sz, size_t &length, std::vector<CodestreamChannelInfo> &map);

#endif /* HT_COMMON_H */
