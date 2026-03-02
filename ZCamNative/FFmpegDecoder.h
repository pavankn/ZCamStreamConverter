#pragma once
#pragma once

#include <vector>
#include <memory>
#include <cstdint>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
}


#define MAX_AV_PLANES 8

// Custom deleters for FFmpeg RAII
struct AVDeleter {
    void operator()(AVCodecContext* p) const { avcodec_free_context(&p); }
    void operator()(AVFrame* p)        const { av_frame_free(&p); }
    void operator()(AVBufferRef* p)    const { av_buffer_unref(&p); }
};

struct audio_output;
typedef struct audio_output audio_t;

enum audio_format {
    AUDIO_FORMAT_UNKNOWN,

    AUDIO_FORMAT_U8BIT,
    AUDIO_FORMAT_16BIT,
    AUDIO_FORMAT_32BIT,
    AUDIO_FORMAT_FLOAT,

    AUDIO_FORMAT_U8BIT_PLANAR,
    AUDIO_FORMAT_16BIT_PLANAR,
    AUDIO_FORMAT_32BIT_PLANAR,
    AUDIO_FORMAT_FLOAT_PLANAR,
};

/**
 * The speaker layout describes where the speakers are located in the room.
 * For OBS it dictates:
 *  *  how many channels are available and
 *  *  which channels are used for which speakers.
 *
 * Standard channel layouts where retrieved from ffmpeg documentation at:
 *     https://trac.ffmpeg.org/wiki/AudioChannelManipulation
 */
enum speaker_layout {
    SPEAKERS_UNKNOWN,     /**< Unknown setting, fallback is stereo. */
    SPEAKERS_MONO,        /**< Channels: MONO */
    SPEAKERS_STEREO,      /**< Channels: FL, FR */
    SPEAKERS_2POINT1,     /**< Channels: FL, FR, LFE */
    SPEAKERS_4POINT0,     /**< Channels: FL, FR, FC, RC */
    SPEAKERS_4POINT1,     /**< Channels: FL, FR, FC, LFE, RC */
    SPEAKERS_5POINT1,     /**< Channels: FL, FR, FC, LFE, RL, RR */
    SPEAKERS_7POINT1 = 8, /**< Channels: FL, FR, FC, LFE, RL, RR, SL, SR */
};

struct audio_data {
    uint8_t* data[MAX_AV_PLANES];
    uint32_t frames;
    uint64_t timestamp;
};

enum video_trc {
    VIDEO_TRC_DEFAULT,
    VIDEO_TRC_SRGB,
    VIDEO_TRC_PQ,
    VIDEO_TRC_HLG,
};

enum video_colorspace {
    VIDEO_CS_DEFAULT,
    VIDEO_CS_601,
    VIDEO_CS_709,
    VIDEO_CS_SRGB,
    VIDEO_CS_2100_PQ,
    VIDEO_CS_2100_HLG,
};

enum video_range_type {
    VIDEO_RANGE_DEFAULT,
    VIDEO_RANGE_PARTIAL,
    VIDEO_RANGE_FULL,
};

struct video_data {
    uint8_t* data[MAX_AV_PLANES];
    uint32_t linesize[MAX_AV_PLANES];
    uint64_t timestamp;
};

struct obs_source_audio {
    const uint8_t* data[MAX_AV_PLANES];
    uint32_t frames;

    enum speaker_layout speakers;
    enum audio_format format;
    uint32_t samples_per_sec;

    uint64_t timestamp;
};

enum video_format {
    VIDEO_FORMAT_NONE,

    /* planar 4:2:0 formats */
    VIDEO_FORMAT_I420, /* three-plane */
    VIDEO_FORMAT_NV12, /* two-plane, luma and packed chroma */

    /* packed 4:2:2 formats */
    VIDEO_FORMAT_YVYU,
    VIDEO_FORMAT_YUY2, /* YUYV */
    VIDEO_FORMAT_UYVY,

    /* packed uncompressed formats */
    VIDEO_FORMAT_RGBA,
    VIDEO_FORMAT_BGRA,
    VIDEO_FORMAT_BGRX,
    VIDEO_FORMAT_Y800, /* grayscale */

    /* planar 4:4:4 */
    VIDEO_FORMAT_I444,

    /* more packed uncompressed formats */
    VIDEO_FORMAT_BGR3,

    /* planar 4:2:2 */
    VIDEO_FORMAT_I422,

    /* planar 4:2:0 with alpha */
    VIDEO_FORMAT_I40A,

    /* planar 4:2:2 with alpha */
    VIDEO_FORMAT_I42A,

    /* planar 4:4:4 with alpha */
    VIDEO_FORMAT_YUVA,

    /* packed 4:4:4 with alpha */
    VIDEO_FORMAT_AYUV,

    /* planar 4:2:0 format, 10 bpp */
    VIDEO_FORMAT_I010, /* three-plane */
    VIDEO_FORMAT_P010, /* two-plane, luma and packed chroma */

    /* planar 4:2:2 format, 10 bpp */
    VIDEO_FORMAT_I210,

    /* planar 4:4:4 format, 12 bpp */
    VIDEO_FORMAT_I412,

    /* planar 4:4:4:4 format, 12 bpp */
    VIDEO_FORMAT_YA2L,

    /* planar 4:2:2 format, 16 bpp */
    VIDEO_FORMAT_P216, /* two-plane, luma and packed chroma */

    /* planar 4:4:4 format, 16 bpp */
    VIDEO_FORMAT_P416, /* two-plane, luma and packed chroma */

    /* packed 4:2:2 format, 10 bpp */
    VIDEO_FORMAT_V210,

    /* packed uncompressed 10-bit format */
    VIDEO_FORMAT_R10L,
};



struct obs_source_frame2 {
    uint8_t* data[MAX_AV_PLANES];
    uint32_t linesize[MAX_AV_PLANES];
    uint32_t width;
    uint32_t height;
    uint64_t timestamp;

    enum video_format format;
    enum video_range_type range;
    float color_matrix[16];
    float color_range_min[3];
    float color_range_max[3];
    bool flip;
    uint8_t flags;
    uint8_t trc; /* enum video_trc */
};

class FfmpegDecoder {
public:
    FfmpegDecoder(AVCodecID id, bool use_hw);
    ~FfmpegDecoder() = default;

    // Prevent copying to avoid double-free of FFmpeg resources
    FfmpegDecoder(const FfmpegDecoder&) = delete;
    FfmpegDecoder& operator=(const FfmpegDecoder&) = delete;

    bool decode_audio(uint8_t* data, size_t size, obs_source_audio* audio, bool* got_output);
    bool decode_video(uint8_t* data, size_t size, long long* ts,
        video_colorspace cs, video_range_type range,
        obs_source_frame2* frame, bool* got_output);

    bool is_valid() const { return decoder != nullptr; }

private:
    void init_hw_decoder();
    void prepare_packet_buffer(uint8_t* data, size_t size);

    std::unique_ptr<AVCodecContext, AVDeleter> decoder;
    std::unique_ptr<AVBufferRef, AVDeleter> hw_device_ctx;
    std::unique_ptr<AVFrame, AVDeleter> frame;
    std::unique_ptr<AVFrame, AVDeleter> hw_frame;

    const AVCodec* codec = nullptr;
    std::vector<uint8_t> packet_buffer;
    bool hw = false;
};