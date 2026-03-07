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
#include <queue>
#include <condition_variable>

enum class DecoderType {
	SOFTWARE,
	HW_CUDA
};

enum class CodecType {
	H264,
	HEVC
};

typedef struct ClientInput {
	std::string ip;
	std::string stream;
	DecoderType decoderType;
	CodecType codecType;
	std::string ndi_name;
	int width;
	int height;
	int fps;
	int vfr;
	int bitrate;
} ClientConfig;


typedef struct VideoPacket {
	std::vector<uint8_t> pkt;
	uint32_t frameno;
}VideoPacket;

struct ClientContext {

	int client_id;
	std::string name;

	ClientConfig clientConfig;

	// Video
	const AVCodec* codec = nullptr;
	AVCodecContext* codec_ctx = nullptr;
	AVBufferRef* hw_device_ctx = nullptr;

	// Audio 
	AVCodecContext* audio_codec_ctx = nullptr;
	const AVCodec* audio_codec = nullptr;
	AVFrame* audio_frame = nullptr;
	uint8_t* audio_buffer = nullptr;
	size_t audio_buffer_size = 0;

	SwsContext* sws_ctx = nullptr;

	AVFrame* rgb_frame = nullptr;

	int last_width = 0;
	int last_height = 0;

	NDIlib_send_instance_t ndi_sender = nullptr;

	// PACKET QUEUE
	std::queue<VideoPacket> video_packet_queue;
	std::mutex video_packet_mutex;
	std::condition_variable video_packet_cv;

	std::thread video_decode_thread;
	std::atomic<bool> running{ true };
};


