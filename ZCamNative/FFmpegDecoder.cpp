#include "FfmpegDecoder.h"
#include <iostream>
#include <stdexcept>


FfmpegDecoder::FfmpegDecoder(AVCodecID id, bool use_hw) {
    codec = avcodec_find_decoder(id);
    if (!codec) throw std::runtime_error("Codec not found");

    decoder.reset(avcodec_alloc_context3(codec));
    if (!decoder) throw std::runtime_error("Context allocation failed");

    decoder->thread_count = 0; // Auto-detect threads

    if (use_hw) init_hw_decoder();

    if (avcodec_open2(decoder.get(), codec, nullptr) < 0)
        throw std::runtime_error("Failed to open codec");

    frame.reset(av_frame_alloc());
    if (hw) hw_frame.reset(av_frame_alloc());
}

void FfmpegDecoder::prepare_packet_buffer(const uint8_t* data, size_t size) {
    size_t total = size + AV_INPUT_BUFFER_PADDING_SIZE;
    if (packet_buffer.size() < total) packet_buffer.resize(total);

    if (data && size) {
        std::copy(data, data + size, packet_buffer.begin());
        std::fill(packet_buffer.begin() + size, packet_buffer.begin() + total, 0);
    }
}

bool FfmpegDecoder::decode_audio(const uint8_t* data, size_t size,
    AudioFrame& out_frame, bool& got_output)
{
    got_output = false;
    int ret = 0;

    // 1. Ensure the frame is allocated
    if (!frame) {
        frame.reset(av_frame_alloc());
        if (!frame) return false;
    }

    // 2. Prepare the padded buffer (replaces manual copy_data)
    prepare_packet_buffer(data, size);

    if (data && size) {
        // RAII Packet management (Standard C++11/14/17 compatible)
        std::unique_ptr<AVPacket, void(*)(AVPacket*)> packet(
            av_packet_alloc(),
            [](AVPacket* p) { av_packet_free(&p); }
        );

        if (!packet) return false;

        packet->data = packet_buffer.data();
        packet->size = static_cast<int>(size);

        ret = avcodec_send_packet(decoder.get(), packet.get());
    }

    if (ret == 0) {
        ret = avcodec_receive_frame(decoder.get(), frame.get());
    }

    // 3. Handle status codes
    bool got_frame = (ret == 0);

    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        ret = 0; // These are expected states, not errors
    }

    if (ret < 0) return false;
    if (!got_frame) return true;

    // 4. Map to our Generic AudioFrame struct
    // We provide the raw pointers; the consumer must process them before 
    // the next call to decode_audio or before the decoder is destroyed.
    for (int i = 0; i < AV_NUM_DATA_POINTERS; i++) {
        out_frame.data[i] = frame->data[i];
    }

    out_frame.samples_per_sec = frame->sample_rate;
    out_frame.nb_samples = frame->nb_samples;
    out_frame.format = static_cast<AVSampleFormat>(frame->format);

    // Use the modern FFmpeg channel count API
    out_frame.channels = decoder->ch_layout.nb_channels;

    got_output = true;
    return true;
}

bool FfmpegDecoder::decode_video(const uint8_t* data, size_t size, long long ts,
    VideoFrame& out_frame, bool& got_output) {
    got_output = false;
    prepare_packet_buffer(data, size);

    std::unique_ptr<AVPacket, void(*)(AVPacket*)> packet(av_packet_alloc(), [](AVPacket* p) { av_packet_free(&p); });
    packet->data = packet_buffer.data();
    packet->size = static_cast<int>(size);
    packet->pts = ts;

    // We removed OBS-specific keyframe detection; FFmpeg usually detects this internally
    int ret = avcodec_send_packet(decoder.get(), packet.get());
    if (ret < 0) return false;

    AVFrame* target = hw ? hw_frame.get() : frame.get();
    ret = avcodec_receive_frame(decoder.get(), target);

    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return true;
    if (ret < 0) return false;

    if (hw) {
        if (av_hwframe_transfer_data(frame.get(), target, 0) < 0) return false;
        target = frame.get();
    }

    // Map to our generic VideoFrame struct
    for (int i = 0; i < AV_NUM_DATA_POINTERS; ++i) {
        out_frame.data[i] = target->data[i];
        out_frame.linesize[i] = target->linesize[i];
    }
    out_frame.width = target->width;
    out_frame.height = target->height;
    out_frame.format = static_cast<AVPixelFormat>(target->format);
    out_frame.pts = target->pts;

    got_output = true;
    return true;
}

bool FfmpegDecoder::init_hw_decoder(ClientContext& ctx) {
    // 1. Determine Decoder Name based on Type and Codec
    std::string decoderName;
    bool is_hw = (ctx.type == DecoderType::HW_CUDA);

    if (is_hw) {
        if (ctx.codecType == CodecType::H264)      decoderName = "h264_cuvid";
        else if (ctx.codecType == CodecType::HEVC) decoderName = "hevc_cuvid";
        else return false;

        ctx.codec = avcodec_find_decoder_by_name(decoderName.c_str());
    }
    else {
        AVCodecID id = (ctx.codecType == CodecType::H264) ? AV_CODEC_ID_H264 : AV_CODEC_ID_HEVC;
        ctx.codec = avcodec_find_decoder(id);
    }

    if (!ctx.codec) {
        std::cerr << "Client[" << ctx.client_id << "] Decoder not found: " << decoderName << std::endl;
        return false;
    }

    // 2. Allocate Context
    ctx.codec_ctx = avcodec_alloc_context3(ctx.codec);
    if (!ctx.codec_ctx) return false;

    // 3. Setup Hardware specific settings
    if (is_hw) {
        if (av_hwdevice_ctx_create(&ctx.hw_device_ctx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0) < 0) {
            std::cerr << "Failed to create CUDA HW device for client " << ctx.client_id << std::endl;
            return false;
        }

        // Link device to codec
        ctx.codec_ctx->hw_device_ctx = av_buffer_ref(ctx.hw_device_ctx);

        // Negotiation callback to tell FFmpeg to use CUDA pixel format
        ctx.codec_ctx->get_format = [](AVCodecContext* /*s*/, const enum AVPixelFormat* pix_fmts) {
            while (*pix_fmts != AV_PIX_FMT_NONE) {
                if (*pix_fmts == AV_PIX_FMT_CUDA)
                    return *pix_fmts;
                pix_fmts++;
            }
            return pix_fmts[0]; // Fallback
            };
    }

    // 4. Open Codec
    if (avcodec_open2(ctx.codec_ctx, ctx.codec, nullptr) < 0) {
        std::cerr << "Could not open codec for client " << ctx.client_id << std::endl;
        return false;
    }

    std::cout << "Client[" << ctx.client_id << "] initialized using "
        << (is_hw ? "HW_CUDA (" + decoderName + ")" : "SOFTWARE") << std::endl;

    return true;
}

void FfmpegDecoder::init_hw_decoder() {
    // 1. Define hardware priority (Portable version of your original array)
    static const enum AVHWDeviceType hw_priority[] = {
        AV_HWDEVICE_TYPE_QSV,           // Intel QuickSync
        AV_HWDEVICE_TYPE_CUDA,          // NVIDIA NVDEC
        AV_HWDEVICE_TYPE_D3D11VA,       // Windows Modern
        AV_HWDEVICE_TYPE_DXVA2,         // Windows Legacy
        AV_HWDEVICE_TYPE_VIDEOTOOLBOX,  // Apple
        AV_HWDEVICE_TYPE_NONE           // Sentinel
    };

    AVBufferRef* hw_ctx = nullptr;

    // 2. Iterate through priorities to find a match the codec supports
    for (const enum AVHWDeviceType* type = hw_priority; *type != AV_HWDEVICE_TYPE_NONE; ++type) {
        bool supported = false;

        // Check if the codec actually supports this specific HW type
        for (int i = 0; ; i++) {
            const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
            if (!config) break;

            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                config->device_type == *type) {
                supported = true;
                break;
            }
        }

        if (supported) {
            // Attempt to initialize the hardware device
            int ret = av_hwdevice_ctx_create(&hw_ctx, *type, nullptr, nullptr, 0);
            if (ret == 0) break; // Successfully initialized
        }
    }

    // 3. If successful, attach the hardware context to our decoder
    if (hw_ctx) {
        hw_device_ctx.reset(hw_ctx); // Transfer ownership to our unique_ptr

        // FFmpeg requires a reference for the decoder context itself
        decoder->hw_device_ctx = av_buffer_ref(hw_device_ctx.get());
        hw = true;
    }
}