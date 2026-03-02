#include "FFmpegDecoder.h"

FfmpegDecoder::FfmpegDecoder(AVCodecID id, bool use_hw)
{
}

bool FfmpegDecoder::decode_audio(uint8_t* data, size_t size, obs_source_audio* audio, bool* got_output)
{
	return false;
}

bool FfmpegDecoder::decode_video(uint8_t* data, size_t size, long long* ts, video_colorspace cs, video_range_type range, obs_source_frame2* frame, bool* got_output)
{
	return false;
}

void FfmpegDecoder::init_hw_decoder()
{
}

void FfmpegDecoder::prepare_packet_buffer(uint8_t* data, size_t size)
{
}
