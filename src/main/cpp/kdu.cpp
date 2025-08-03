/*
Copyright (c) 2024, Sandflow Consulting LLC

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <limits>
#include <string>
#include <fstream>
#include <iostream>
#include <vector>

#include "kdu.h"
#include "internal_ht_common.h"

#include "kdu_elementary.h"
#include "kdu_params.h"
#include "kdu_stripe_compressor.h"
#include "kdu_compressed.h"
#include "kdu_file_io.h"
#include "kdu_messaging.h"
#include "kdu_sample_processing.h"
#include "kdu_stripe_decompressor.h"

using namespace kdu_supp;

class mem_compressed_target : public kdu_compressed_target
{
public:
    mem_compressed_target(void *buf, size_t buf_size)
    {
        this->max_size = buf_size;
        this->used_size = 0;
        this->buf = (uint8_t *)buf;
        this->cur_ptr = this->buf;
    }

    bool close()
    {
        this->buf = NULL;
        this->cur_ptr = NULL;
        this->max_size = 0;
        this->used_size = 0;
        return true;
    }

    bool write(const kdu_byte *ptr, int sz)
    {
        size_t needed_size = (size_t)(this->cur_ptr - this->buf) + sz; //needed size
        if (needed_size > this->max_size)
        {
            throw std::range_error("Buffer size exceeded");
        }

        // copy bytes into buffer and adjust cur_ptr
        memcpy(this->cur_ptr, ptr, sz);
        this->cur_ptr += sz;
        used_size = needed_size;

        return sz;
    }

    void set_target_size(kdu_long num_bytes)
    {
        if (num_bytes > this->max_size)
        {
            throw std::range_error("Buffer size exceeded");
        }
    }

    bool prefer_large_writes() const { return false; }

    uint8_t *get_buffer() { return this->buf; }

    size_t get_size() { return this->used_size; }

private:
    uint8_t *buf;
    size_t max_size;
    size_t used_size;
    uint8_t *cur_ptr;
};

class error_message_handler : public kdu_core::kdu_message
{
public:
    void put_text(const char *msg) { std::cout << msg; }

    virtual void flush(bool end_of_message = false)
    {
        if (end_of_message)
        {
            std::cout << std::endl;
        }
    }
};

static error_message_handler error_handler;

extern "C" exr_result_t
kdu_decompress(
    exr_decode_pipeline_t *decode)
{
    exr_result_t rv = EXR_ERR_SUCCESS;

    if (decode->chunk.packed_size == 0)
        return EXR_ERR_SUCCESS;

    if (decode->chunk.packed_size == decode->chunk.unpacked_size)
    {
        if (decode->unpacked_buffer != decode->packed_buffer)
            memcpy(decode->unpacked_buffer, decode->packed_buffer, decode->chunk.packed_size);
        return EXR_ERR_SUCCESS;
    }

    std::vector<CodestreamChannelInfo> cs_to_file_ch(decode->channel_count);

    /* read the channel map */

    size_t header_sz = read_header(
        (uint8_t *)decode->packed_buffer, decode->chunk.packed_size, cs_to_file_ch);
    if (decode->channel_count != cs_to_file_ch.size())
        throw std::runtime_error("Unexpected number of channels");

    std::vector<int> heights(decode->channel_count);
    std::vector<int> sample_offsets(decode->channel_count);

    int32_t width = decode->chunk.width;
    int32_t height = decode->chunk.height;

    for (int i = 0; i < sample_offsets.size(); i++)
    {
        sample_offsets[i] = cs_to_file_ch[i].file_index * width;
    }

    std::vector<int> row_gaps(decode->channel_count);
    std::fill(
        row_gaps.begin(), row_gaps.end(), width * decode->channel_count);

    kdu_core::kdu_customize_errors(&error_handler);

    kdu_compressed_source_buffered infile(
        ((kdu_byte *)(decode->packed_buffer)) + header_sz, decode->chunk.packed_size - header_sz);

    kdu_codestream cs;
    cs.create(&infile);

    kdu_dims dims;
    cs.get_dims(0, dims, false);

    assert(width == dims.size.x);
    assert(height == dims.size.y);
    assert(decode->channel_count == cs.get_num_components());
    assert(sizeof(int16_t) == 2);

    kdu_stripe_decompressor d;

    d.start(cs);

    std::fill(heights.begin(), heights.end(), height);

    if (decode->channels[0].data_type == EXR_PIXEL_HALF)
    {
        d.pull_stripe(
            (kdu_int16 *)decode->unpacked_buffer,
            heights.data(),
            sample_offsets.data(),
            NULL,
            row_gaps.data());
    }
    else
    {
        d.pull_stripe(
            (kdu_int32 *)decode->unpacked_buffer,
            heights.data(),
            sample_offsets.data(),
            NULL,
            row_gaps.data());
    }

    d.finish();

    cs.destroy();

    return rv;
}

extern "C" exr_result_t
kdu_compress(exr_encode_pipeline_t *encode)
{
    exr_result_t rv = EXR_ERR_SUCCESS;

    std::vector<CodestreamChannelInfo> cs_to_file_ch(encode->channel_count);
    bool isRGB = make_channel_map(
        encode->channel_count, encode->channels, cs_to_file_ch);

    int height = encode->chunk.height;
    int width = encode->chunk.width;

    std::vector<int> heights(encode->channel_count);
    std::fill(heights.begin(), heights.end(), height);

    std::vector<int> sample_offsets(encode->channel_count);
    for (int i = 0; i < sample_offsets.size(); i++)
    {
        sample_offsets[i] = cs_to_file_ch[i].file_index * width;
    }

    std::vector<int> row_gaps(encode->channel_count);
    std::fill(
        row_gaps.begin(), row_gaps.end(), width * encode->channel_count);

    siz_params siz;
    siz.set(Scomponents, 0, 0, encode->channel_count);
    siz.set(Sdims, 0, 0, height);
    siz.set(Sdims, 0, 1, width);
    siz.set(Nprecision, 0, 0, encode->channels[0].data_type == EXR_PIXEL_HALF ? 16 : 32);
    siz.set(Nsigned, 0, 0, encode->channels[0].data_type != EXR_PIXEL_UINT);
    static_cast<kdu_params &>(siz).finalize();

    size_t header_sz = write_header(
        (uint8_t *)encode->compressed_buffer,
        encode->packed_bytes,
        cs_to_file_ch);

    kdu_codestream codestream;
    mem_compressed_target output(((uint8_t *)encode->compressed_buffer) + header_sz, encode->packed_bytes - header_sz);

    try
    {

        codestream.create(&siz, &output);

        codestream.set_disabled_auto_comments(0xFFFFFFFF);

        kdu_params *cod = codestream.access_siz()->access_cluster(COD_params);

        cod->set(Creversible, 0, 0, true);
        cod->set(Corder, 0, 0, Corder_RPCL);
        cod->set(Cmodes, 0, 0, Cmodes_HT);
        cod->set(Cblk, 0, 0, 32);
        cod->set(Cblk, 0, 1, 128);
        cod->set(Clevels, 0, 0, 5);
        cod->set(Cycc, 0, 0, isRGB);

        if (encode->channels[0].data_type != EXR_PIXEL_UINT)
        {
            kdu_params *nlt = codestream.access_siz()->access_cluster(NLT_params);
            nlt->set(NLType, 0, 0, NLType_SMAG);
        }

        codestream.access_siz()->finalize_all();

        kdu_stripe_compressor compressor;
        compressor.start(codestream);

        if (encode->channels[0].data_type == EXR_PIXEL_HALF)
        {
            compressor.push_stripe(
                (kdu_int16 *)encode->packed_buffer,
                heights.data(),
                sample_offsets.data(),
                NULL,
                row_gaps.data());
        }
        else
        {
            compressor.push_stripe(
                (kdu_int32 *)encode->packed_buffer,
                heights.data(),
                sample_offsets.data(),
                NULL,
                row_gaps.data());
        }

        compressor.finish();

        codestream.destroy();

        encode->compressed_bytes = output.get_size() + header_sz;
    }
    catch (const std::range_error &e)
    {
        encode->compressed_bytes = encode->packed_bytes;
    }

    return rv;
}
