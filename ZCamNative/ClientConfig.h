#pragma once
#include <iostream>

enum class DecoderType {
	SOFTWARE,
	HW_CUDA
};

enum class HWCodecType {
	H264_CUVID,
	HEVC_CUVID
};

typedef struct ClientConfig {
	std::string ip;
	std::string stream;
	DecoderType decoder_type;
	HWCodecType hwCodecType;
	std::string ndi_name;
	int width;
	int height;
}ClientConfig;