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
        "file", "Input image", cxxopts::value<std::string>());

    options.parse_positional({"file"});

    options.show_positional_help();

    auto args = options.parse(argc, argv);

    if (args.count("file") != 1)
    {
        std::cout << options.help() << std::endl;
        exit(-1);
    }

    auto &src_fn = args["file"].as<std::string>();

    /* load src image */

    exr_result_t r;

    exr_context_initializer_t ctxtinit = EXR_DEFAULT_CONTEXT_INITIALIZER;

    /* source file */

    exr_context_t src_file;
    dif(exr_start_read(&src_file, src_fn.c_str(), &ctxtinit));

    int partCount;
    dif(exr_get_count(src_file, &partCount));

    /* destination file */


    for (int part_id = 0; part_id < partCount; part_id++)
    {
        exr_storage_t stortype;
        dif(exr_get_storage(src_file, part_id, &stortype));
        if (stortype != EXR_STORAGE_SCANLINE)
        {
            std::cout << "Only supports scanline files" << std::endl;
            exit(-1);
        }

        int32_t scansperchunk;
        dif(exr_get_scanlines_per_chunk(src_file, part_id, &scansperchunk));

        exr_attr_box2i_t dw;
        dif(exr_get_data_window(src_file, part_id, &dw));
        int width = dw.max.x - dw.min.x + 1;
        int height = dw.max.y - dw.min.y + 1;

        const exr_attr_chlist_t *channels;
        dif(exr_get_channels (src_file, part_id, &channels));

        /* allocate decoded chunk memory */
        if (channels->num_channels > MAX_CHANNEL_COUNT) {
            std::cout << "Max channel count exceeded" << std::endl;
            exit(-1);
        }

        uint8_t pixelstride = 0;
        uint8_t ch_offset[MAX_CHANNEL_COUNT];
        for (int ch_id = 0; ch_id < channels->num_channels; ++ch_id) {
            ch_offset[ch_id] = pixelstride;
            pixelstride += channels->entries[ch_id].pixel_type == EXR_PIXEL_HALF ? 2 : 4;
        }
        int32_t linestride = pixelstride * width;
        uint8_t *baseband_buf = (uint8_t *)malloc(scansperchunk * width * pixelstride);

        bool first = true;
        exr_decode_pipeline_t decoder;
        exr_chunk_info_t chunk;
        for (int y = dw.min.y; y <= dw.max.y; y += scansperchunk)
        {
            dif(exr_read_scanline_chunk_info(src_file, part_id, y, &chunk));
            if (first)
            {
                dif(exr_decoding_initialize(src_file, part_id, &chunk, &decoder));
            }
            else
            {
                dif(exr_decoding_update(src_file, part_id, &chunk, &decoder));
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

                decoder.channels[ch_id].decode_to_ptr = baseband_buf + ch_offset[ch_id];
                decoder.channels[ch_id].user_pixel_stride = pixelstride;
                decoder.channels[ch_id].user_line_stride = linestride;
            }

            if (first)
            {
                dif(
                    exr_decoding_choose_default_routines(src_file, part_id, &decoder));
                decoder.decompress_fn = kdu_decompress;
            }
            dif(exr_decoding_run(src_file, part_id, &decoder));

            first = false;
        }
        dif(exr_decoding_destroy(src_file, &decoder));
        free(baseband_buf);
    }

    dif(exr_finish(&src_file));

    return 0;
}