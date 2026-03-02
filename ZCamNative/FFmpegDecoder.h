#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
}

#include "ClientProcessor.h"

using namespace com_khelai_zcamnative;

// RAII Deleter for FFmpeg types
struct AVDeleter {
    void operator()(AVCodecContext* p) const { avcodec_free_context(&p); }
    void operator()(AVFrame* p)        const { av_frame_free(&p); }
    void operator()(AVBufferRef* p)    const { av_buffer_unref(&p); }
};

// Generic output structures to replace OBS types
struct VideoFrame {
    uint8_t* data[AV_NUM_DATA_POINTERS];
    int linesize[AV_NUM_DATA_POINTERS];
    int width, height;
    AVPixelFormat format;
    long long pts;
};

struct AudioFrame {
    uint8_t* data[AV_NUM_DATA_POINTERS];
    int samples_per_sec;
    int channels;
    int nb_samples;
    AVSampleFormat format;
};

class FfmpegDecoder {
public:
    FfmpegDecoder(AVCodecID id, bool use_hw);
    ~FfmpegDecoder() = default;

    bool decode_video(const uint8_t* data, size_t size, long long ts,
        VideoFrame& out_frame, bool& got_output);

    bool decode_audio(const uint8_t* data, size_t size,
        AudioFrame& out_frame, bool& got_output);

private:
    void init_hw_decoder();
    bool init_hw_decoder(ClientContext& ctx);
    void prepare_packet_buffer(const uint8_t* data, size_t size);

    std::unique_ptr<AVCodecContext, AVDeleter> decoder;
    std::unique_ptr<AVBufferRef, AVDeleter> hw_device_ctx;
    std::unique_ptr<AVFrame, AVDeleter> frame;
    std::unique_ptr<AVFrame, AVDeleter> hw_frame;

    const AVCodec* codec = nullptr;
    std::vector<uint8_t> packet_buffer;
    bool hw = false;
};