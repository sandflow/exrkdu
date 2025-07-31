#include <string>
#include <map>
#include <chrono>
#include <numeric>
#include <cmath>
#include <iterator>
#include <mutex>

#include <openexr.h>
#include "kdu.h"

#include "cxxopts.hpp"

#define MAX_CHANNEL_COUNT 32
#define MAX_PART_COUNT 128

void dif(exr_result_t r)
{
    if (r != EXR_ERR_SUCCESS)
    {
        printf("fail");
        exit(-1);
    }
}

int main(int argc, char *argv[])
{
    cxxopts::Options options(
        "exrkdu", "Demonstrates how to use KDU SDK with the OpenEXR C API");

    options.add_options()(
        "ipath", "Input image path", cxxopts::value<std::string>())(
        "epath", "Encoded image path", cxxopts::value<std::string>());

    options.parse_positional({"ipath", "epath"});

    options.show_positional_help();

    auto args = options.parse(argc, argv);

    if (args.count("ipath") != 1 || args.count("epath") != 1)
    {
        std::cout << options.help() << std::endl;
        exit(-1);
    }

    auto &src_fn = args["ipath"].as<std::string>();
    auto &enc_fn = args["epath"].as<std::string>();

    exr_result_t r;

    /* source file */

    exr_context_t src_file;
    dif(exr_start_read(&src_file, src_fn.c_str(), NULL));

    int partCount;
    dif(exr_get_count(src_file, &partCount));

    if (partCount > MAX_PART_COUNT)
    {
        std::cout << "Max part count exceeded" << std::endl;
        exit(-1);
    }

    /* encoded file */

    exr_context_t enc_file;
    dif(exr_start_write(&enc_file, enc_fn.c_str(), EXR_WRITE_FILE_DIRECTLY, NULL));

    /* copy parts to the output file */

    for (int part_id = 0; part_id < partCount; part_id++)
    {
        exr_storage_t stortype;
        dif(exr_get_storage(src_file, part_id, &stortype));
        if (stortype != EXR_STORAGE_SCANLINE)
        {
            std::cout << "Only supports scanline files" << std::endl;
            exit(-1);
        }

        const char *pn = NULL;
        exr_get_name(src_file, part_id, &pn);

        int new_part_id = 0;
        dif(exr_add_part(enc_file, pn, EXR_STORAGE_SCANLINE, &new_part_id));

        if (new_part_id != part_id)
        {
            std::cout << "Part index mismatch" << std::endl;
            exit(-1);
        }

        dif(exr_copy_unset_attributes(enc_file, part_id, src_file, part_id));
        dif(exr_set_compression(enc_file, part_id, EXR_COMPRESSION_HTJ2K));
    }
    dif(exr_write_header(enc_file));

    /* baseband buffers */

    uint8_t *baseband_bufs[MAX_PART_COUNT] = {NULL};

    /* generated encoded file */

    for (int part_id = 0; part_id < partCount; part_id++)
    {
        exr_attr_box2i_t dw;
        dif(exr_get_data_window(src_file, part_id, &dw));
        int width = dw.max.x - dw.min.x + 1;
        int height = dw.max.y - dw.min.y + 1;

        const exr_attr_chlist_t *channels;
        dif(exr_get_channels(src_file, part_id, &channels));

        /* allocate basband image buffer */
        uint8_t pixelstride = 0;
        if (channels->num_channels > MAX_CHANNEL_COUNT)
        {
            std::cout << "Max channel count exceeded" << std::endl;
            exit(-1);
        }
        uint8_t ch_offset[MAX_CHANNEL_COUNT];
        for (int ch_id = 0; ch_id < channels->num_channels; ++ch_id)
        {
            ch_offset[ch_id] = pixelstride;
            pixelstride += channels->entries[ch_id].pixel_type == EXR_PIXEL_HALF ? 2 : 4;
        }
        int32_t linestride = pixelstride * width;
        baseband_bufs[part_id] = (uint8_t *)malloc(height * width * pixelstride);

        /* decode */

        bool first = true;
        exr_decode_pipeline_t decoder;
        exr_chunk_info_t dec_chunk;
        int32_t scansperchunk;
        dif(exr_get_scanlines_per_chunk(src_file, part_id, &scansperchunk));
        uint8_t *chunk_buf = baseband_bufs[part_id];
        for (int y = dw.min.y; y <= dw.max.y; y += scansperchunk)
        {
            dif(exr_read_scanline_chunk_info(src_file, part_id, y, &dec_chunk));

            if (first)
            {
                dif(exr_decoding_initialize(src_file, part_id, &dec_chunk, &decoder));
            }
            else
            {
                dif(exr_decoding_update(src_file, part_id, &dec_chunk, &decoder));
            }

            for (int ch_id = 0; ch_id < decoder.channel_count; ++ch_id)
            {
                const exr_coding_channel_info_t &channel = decoder.channels[ch_id];

                if (channel.height == 0)
                {
                    decoder.channels[ch_id].decode_to_ptr = NULL;
                    decoder.channels[ch_id].user_pixel_stride = 0;
                    decoder.channels[ch_id].user_line_stride = 0;
                    continue;
                }

                decoder.channels[ch_id].decode_to_ptr = chunk_buf + ch_offset[ch_id];
                decoder.channels[ch_id].user_pixel_stride = pixelstride;
                decoder.channels[ch_id].user_line_stride = linestride;
            }

            if (first)
            {
                dif(
                    exr_decoding_choose_default_routines(src_file, part_id, &decoder));
            }
            dif(exr_decoding_run(src_file, part_id, &decoder));

            first = false;
            chunk_buf += linestride * scansperchunk;
        }
        dif(exr_decoding_destroy(src_file, &decoder));

        /* encode */

        first = true;
        exr_encode_pipeline_t encoder;
        exr_chunk_info_t enc_chunk;
        dif(exr_get_scanlines_per_chunk(enc_file, part_id, &scansperchunk));
        chunk_buf = baseband_bufs[part_id];
        for (int y = dw.min.y; y <= dw.max.y; y += scansperchunk)
        {
            dif(exr_write_scanline_chunk_info(enc_file, part_id, y, &enc_chunk));

            if (first)
            {
                dif(exr_encoding_initialize(enc_file, part_id, &enc_chunk, &encoder));
            }
            else
            {
                dif(exr_encoding_update(enc_file, part_id, &enc_chunk, &encoder));
            }

            for (int ch_id = 0; ch_id < encoder.channel_count; ++ch_id)
            {
                const exr_coding_channel_info_t &channel = encoder.channels[ch_id];

                if (channel.height == 0)
                {
                    encoder.channels[ch_id].encode_from_ptr = NULL;
                    encoder.channels[ch_id].user_pixel_stride = 0;
                    encoder.channels[ch_id].user_line_stride = 0;
                    continue;
                }

                encoder.channels[ch_id].encode_from_ptr = chunk_buf + ch_offset[ch_id];
                encoder.channels[ch_id].user_pixel_stride = pixelstride;
                encoder.channels[ch_id].user_line_stride = linestride;
            }

            if (first)
            {
                dif(
                    exr_encoding_choose_default_routines(enc_file, part_id, &encoder));
                encoder.compressed_bytes = scansperchunk * linestride;
                encoder.compressed_buffer = malloc(encoder.compressed_bytes);
                encoder.compress_fn = kdu_compress;
            }
            dif(exr_encoding_run(enc_file, part_id, &encoder));

            first = false;
            chunk_buf += linestride * scansperchunk;
        }
        free(encoder.compressed_buffer);
        dif(exr_encoding_destroy(enc_file, &encoder));
    }

    dif(exr_finish(&src_file));
    dif(exr_finish(&enc_file));

    /* read and compare with baseband */

    exr_context_t dec_file;
    dif(exr_start_read(&dec_file, enc_fn.c_str(), NULL));

    for (int part_id = 0; part_id < partCount; part_id++)
    {
        exr_attr_box2i_t dw;
        dif(exr_get_data_window(dec_file, part_id, &dw));
        int width = dw.max.x - dw.min.x + 1;
        int height = dw.max.y - dw.min.y + 1;

        /* allocate decoded image buffer */

        const exr_attr_chlist_t *channels;
        dif(exr_get_channels(dec_file, part_id, &channels));
        uint8_t pixelstride = 0;
        if (channels->num_channels > MAX_CHANNEL_COUNT)
        {
            std::cout << "Max channel count exceeded" << std::endl;
            exit(-1);
        }
        uint8_t ch_offset[MAX_CHANNEL_COUNT];
        for (int ch_id = 0; ch_id < channels->num_channels; ++ch_id)
        {
            ch_offset[ch_id] = pixelstride;
            pixelstride += channels->entries[ch_id].pixel_type == EXR_PIXEL_HALF ? 2 : 4;
        }
        int32_t linestride = pixelstride * width;
        uint8_t*  dec_buffer = (uint8_t *)malloc(height * width * pixelstride);

        /* decode */

        bool first = true;
        exr_decode_pipeline_t decoder;
        exr_chunk_info_t dec_chunk;
        int32_t scansperchunk;
        dif(exr_get_scanlines_per_chunk(dec_file, part_id, &scansperchunk));
        uint8_t *chunk_buf = dec_buffer;
        for (int y = dw.min.y; y <= dw.max.y; y += scansperchunk)
        {
            dif(exr_read_scanline_chunk_info(dec_file, part_id, y, &dec_chunk));

            if (first)
            {
                dif(exr_decoding_initialize(dec_file, part_id, &dec_chunk, &decoder));
            }
            else
            {
                dif(exr_decoding_update(dec_file, part_id, &dec_chunk, &decoder));
            }

            for (int ch_id = 0; ch_id < decoder.channel_count; ++ch_id)
            {
                const exr_coding_channel_info_t &channel = decoder.channels[ch_id];

                if (channel.height == 0)
                {
                    decoder.channels[ch_id].decode_to_ptr = NULL;
                    decoder.channels[ch_id].user_pixel_stride = 0;
                    decoder.channels[ch_id].user_line_stride = 0;
                    continue;
                }

                decoder.channels[ch_id].decode_to_ptr = chunk_buf + ch_offset[ch_id];
                decoder.channels[ch_id].user_pixel_stride = pixelstride;
                decoder.channels[ch_id].user_line_stride = linestride;
            }

            if (first)
            {
                dif(
                    exr_decoding_choose_default_routines(dec_file, part_id, &decoder));
                decoder.decompress_fn = kdu_decompress;
            }
            dif(exr_decoding_run(dec_file, part_id, &decoder));

            first = false;
            chunk_buf += linestride * scansperchunk;
        }
        dif(exr_decoding_destroy(dec_file, &decoder));

        /* compare with baseband */

        if (memcmp(baseband_bufs[part_id], dec_buffer, height * width * pixelstride)) {
            std::cout << "Decoded image does not match the source image" << std::endl;
            exit(-1);
        }

        free(dec_buffer);
    }

    dif(exr_finish(&dec_file));

    /* free baseband buffers */

    for (int part_id = 0; part_id < partCount; part_id++)
    {
        free(baseband_bufs[part_id]);
    }

    std::cout << "Success" << std::endl;

    return 0;
}