#pragma once

#include <iostream>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavcodec/bsf.h>
}

#include <Processing.NDI.Lib.h>
#include <mutex>


enum class DecoderType {
	SOFTWARE,
	HW_CUDA
};

enum class CodecType {
	H264,
	HEVC
};

typedef struct ClientContext {
	int client_id;       // Unique ID per client
	std::string name;    // NDI sender name

	DecoderType type = DecoderType::SOFTWARE;
	CodecType   codecType = CodecType::H264;
	// FFmpeg decoding
	const AVCodec* codec = nullptr;
	AVCodecContext* codec_ctx = nullptr;
	AVBufferRef* hw_device_ctx = nullptr;  // Only used in HW mode

	// Conversion (YUV to BGRA)
	SwsContext* sws_ctx = nullptr;

	// RGB frame for rendering or NDI
	AVFrame* rgb_frame = nullptr;
	int last_width = 0;
	int last_height = 0;

	// NDI sender
	NDIlib_send_instance_t ndi_sender = nullptr;

	// Thread safety
	std::mutex decoder_mutex;

	// Members for general decode paths, holding CPU-resident data
	AVFrame* host_frame_bgr = nullptr; // Will hold CPU-resident BGR data after sws_scale
	SwsContext* sws_ctx_yuv_to_bgr = nullptr; // For YUV to BGR conversion on CPU


	// No need for ctx.rgb_frame (AVFrame) or ctx.sws_ctx (SwsContext) for the CUDA path
	// if you fully offload to GPU. Keep them for SW decode path if still supported.

	// A flag to indicate if CUDA resources are initialized/allocated
	bool cuda_resources_initialized = false;
}ClientContext;


typedef struct {
	std::string ip;
	std::string stream;
	DecoderType decoderType;
	CodecType codecType;
	std::string ndi_name;
	int width;
	int height;
	int fps;
	int vfr;
} ClientInput;