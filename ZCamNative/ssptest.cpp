#define _GLIBCXX_USE_CXX11_ABI 1
#include <functional>
#include <memory>
#include <thread>

#include <stdlib.h>
#include <iostream>
#include "imf/net/loop.h"
#include "imf/net/threadloop.h"
#include "imf/ssp/sspclient.h"
#include <mutex>
#include <vector>
#include <string>
#include <curl/curl.h>
#include <algorithm>  // for std::min
#include <map>
#include <cstdio>
#include <fcntl.h>
#include "StreamParser.h"

using namespace std::placeholders;
using namespace com_khelai_zcamnative;

#ifdef _DEBUG
#pragma comment (lib, "libsspd.lib")
#else
#pragma comment (lib, "libssp.lib")
#endif

std::atomic<bool> running(true);
std::mutex out_mutex;
imf::Loop* gLoop = nullptr;


static std::vector<std::unique_ptr<ClientContext>> g_client_contexts;
static std::vector<std::unique_ptr<imf::SspClient>> g_ssp_clients;

static size_t WriteCallback(void* contents, size_t size,
	size_t nmemb, void* userp)
{
	((std::string*)userp)->append(
		(char*)contents, size * nmemb);
	return size * nmemb;
}

class ZCamStreamBuilder {
public:
	ZCamStreamBuilder(const std::string& ip)
		: ip_(ip) {
		params_["index"] = "stream1"; // default
	}

	ZCamStreamBuilder& index(const std::string& index) {
		params_["index"] = index;
		return *this;
	}

	ZCamStreamBuilder& resolution(int w, int h) {
		params_["width"] = std::to_string(w);
		params_["height"] = std::to_string(h);
		return *this;
	}

	ZCamStreamBuilder& bitrate(uint32_t bps) {
		params_["bitrate"] = std::to_string(bps);
		return *this;
	}

	ZCamStreamBuilder& encoder(const std::string& enc) {
		params_["venc"] = enc;
		return *this;
	}

	ZCamStreamBuilder& fps(int value) {
		params_["fps"] = std::to_string(value);
		return *this;
	}

	bool apply()
	{
		Logger log("zcam_native.log");
				
		CURL* curl = curl_easy_init();
		if (!curl) return false;

		std::string response;

		std::string url =
			"http://" + ip_ + "/ctrl/stream_setting";

		bool first = true;
		for (const auto& pair : params_)
		{
			char* enc =
				curl_easy_escape(curl,
					pair.second.c_str(), 0);

			url += (first ? "?" : "&") +
				pair.first + "=" + enc;

			curl_free(enc);
			first = false;
		}

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");

		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		if (res != CURLE_OK)
			return false;

		// SUCCESS
		if (response.find("\"code\":0") != std::string::npos) {
			return true;
		}

		return false;
	}
	bool set_vfr(int vfr)
	{
		CURL* curl = curl_easy_init();
		if (!curl) return false;

		std::string response;

		std::string url =
			"http://" + ip_ + "/ctrl/set?movvfr=" + std::to_string(vfr);

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		return (res == CURLE_OK &&
			response.find("\"code\":0") != std::string::npos);
	}
	bool set_resolution(std::string resolution)
	{
		CURL* curl = curl_easy_init();
		if (!curl) return false;

		std::string response;

		std::string url =
			"http://" + ip_ + "/ctrl/set?resolution=" + resolution;

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		return (res == CURLE_OK &&
			response.find("\"code\":0") != std::string::npos);
	}
	bool set_video_encoder(std::string videoencoder)
	{
		CURL* curl = curl_easy_init();
		if (!curl) return false;

		std::string response;

		std::string url =
			"http://" + ip_ + "/ctrl/set?video_encoder=" + videoencoder;

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		return (res == CURLE_OK &&
			response.find("\"code\":0") != std::string::npos);
	}

	bool set_stream(std::string streamindex)
	{
		CURL* curl = curl_easy_init();
		if (!curl) return false;

		std::string response;

		std::string url =
			"http://" + ip_ + "/ctrl/set?send_stream=" + streamindex;

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		return (res == CURLE_OK &&
			response.find("\"code\":0") != std::string::npos);
	}

private:
	std::string ip_;
	std::map<std::string, std::string> params_;
};


bool init_decoder(ClientContext& ctx) {
	Logger log("zcam_native.log");

	std::string decoderName;
	DecoderType decoderType = ctx.clientConfig.decoderType;
	CodecType codecType = ctx.clientConfig.codecType;

	// Video
	if (decoderType == DecoderType::HW_CUDA) {
		if (codecType == CodecType::H264) {
			decoderName = "h264_cuvid";
		}
		else if (codecType == CodecType::HEVC) {
			decoderName = "hevc_cuvid";
		}
		else {
			return false;
		}
		ctx.codec = avcodec_find_decoder_by_name(decoderName.c_str());  // or "h264_nvdec"
		if (!ctx.codec) {
			log.error("Client[{}] CUDA decoder not found", ctx.client_id);
			return false;
		}

		ctx.codec_ctx = avcodec_alloc_context3(ctx.codec);
		if (!ctx.codec_ctx) return false;

		if (av_hwdevice_ctx_create(&ctx.hw_device_ctx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0) < 0) {
			log.error("Failed to create CUDA HW device for client {}", ctx.client_id);
			return false;
		}
		ctx.codec_ctx->hw_device_ctx = av_buffer_ref(ctx.hw_device_ctx);

		ctx.codec_ctx->get_format = [](AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts) {
			while (*pix_fmts != AV_PIX_FMT_NONE) {
				if (*pix_fmts == AV_PIX_FMT_CUDA)
					return *pix_fmts;
				pix_fmts++;
			}
			return pix_fmts[0];
			};
	}
	else {  // SOFTWARE
		if (codecType == CodecType::H264) {
			ctx.codec = avcodec_find_decoder(AV_CODEC_ID_H264);
		}
		else if (codecType == CodecType::HEVC) {
			ctx.codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
		}
		else {
			return false;
		}

		if (!ctx.codec) {
			log.error("Client[{}] software decoder not found", ctx.client_id);
			return false;
		}

		ctx.codec_ctx = avcodec_alloc_context3(ctx.codec);
		if (!ctx.codec_ctx) return false;
	}

	if (avcodec_open2(ctx.codec_ctx, ctx.codec, nullptr) < 0) {
		log.error("Could not open codec for client {}", ctx.client_id);
		return false;
	}

	log.info("Client {} Video decoder {} initialized ", ctx.client_id,
		(decoderType == DecoderType::HW_CUDA ? "HW_CUDA" : "SOFTWARE"));

	// Audio
	ctx.audio_codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
	ctx.audio_codec_ctx = avcodec_alloc_context3(ctx.audio_codec);
	if (avcodec_open2(ctx.audio_codec_ctx, ctx.audio_codec, nullptr) < 0)
	{
		log.error("Failed to open audio decoder\n");
		return false;
	}
	log.info("Client {} AudioDecoder Initialized", ctx.client_id);

	return true;
}

// Don't forget to add a cleanup function for ClientContext to free CUDA resources
void cleanup_client_context_cuda(ClientContext& ctx) {
	/*if (ctx.d_bgra_frame) {
		CHECK_CUDA(cudaFree(ctx.d_bgra_frame));
		ctx.d_bgra_frame = nullptr;
	}*/
	// Also free AVFrame and SwsContext if they are kept for SW path
	if (ctx.rgb_frame) {
		av_frame_free(&ctx.rgb_frame);
		ctx.rgb_frame = nullptr;
	}
	if (ctx.sws_ctx) {
		sws_freeContext(ctx.sws_ctx);
		ctx.sws_ctx = nullptr;
	}
	// Don't free ctx.codec_ctx or ctx.ndi_sender here, as they are managed externally
}

static void send_audio_to_ndi(ClientContext* ctx, AVFrame* frame)
{
	NDIlib_audio_frame_v3_t ndi_audio = { 0 };

	int samples = frame->nb_samples;
	int channels = frame->ch_layout.nb_channels;

	size_t stride = samples * sizeof(float);
	size_t required = channels * stride;

	// grow buffer if needed
	if (required > ctx->audio_buffer_size)
	{
		if (ctx->audio_buffer)
			free(ctx->audio_buffer);

		ctx->audio_buffer = (uint8_t*)malloc(required);
		ctx->audio_buffer_size = required;
	}

	// copy planar channels
	for (int c = 0; c < channels; c++)
	{
		memcpy(
			ctx->audio_buffer + c * stride,
			frame->data[c],
			stride
		);
	}

	ndi_audio.sample_rate = frame->sample_rate;
	ndi_audio.no_channels = channels;
	ndi_audio.no_samples = samples;

	ndi_audio.channel_stride_in_bytes = stride;

	ndi_audio.timecode = NDIlib_send_timecode_synthesize;

	ndi_audio.p_data = ctx->audio_buffer;

	NDIlib_send_send_audio_v3(ctx->ndi_sender, &ndi_audio);
}

void decode_audio(ClientContext* ctx, const std::vector<uint8_t>& pkt)
{
	AVPacket* packet = av_packet_alloc();

	packet->data = (uint8_t*)pkt.data();
	packet->size = pkt.size();

	if (avcodec_send_packet(ctx->audio_codec_ctx, packet) < 0)
	{
		av_packet_free(&packet);
		return;
	}

	if (!ctx->audio_frame)
		ctx->audio_frame = av_frame_alloc();

	static int index = 0;

	while (true)
	{
		int ret = avcodec_receive_frame(
			ctx->audio_codec_ctx,
			ctx->audio_frame
		);

		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			break;

		if (ret < 0)
			break;

		send_audio_to_ndi(ctx, ctx->audio_frame);		
	}

	av_packet_free(&packet);
}

void decode_video(ClientContext* ctx, VideoPacket* pkt)
{
	static Logger log("zcam_native.log");

	if (!ctx || !pkt || !ctx->running)
		return;

	AVPacket* avpkt = av_packet_alloc();
	av_new_packet(avpkt, pkt->len);
	memcpy(avpkt->data, pkt->data, pkt->len);

	if (avcodec_send_packet(ctx->codec_ctx, avpkt) < 0)
	{
		av_packet_free(&avpkt);
		free(pkt->data);
		return;
	}

	AVFrame* frame = av_frame_alloc();
	AVFrame* hw_frame = nullptr;

	while (true)
	{
		int ret = avcodec_receive_frame(ctx->codec_ctx, frame);

		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			break;

		if (ret < 0)
			break;

		AVFrame* use_frame = frame;

		if (ctx->clientConfig.decoderType == DecoderType::HW_CUDA)
		{
			if (!hw_frame)
				hw_frame = av_frame_alloc();

			if (av_hwframe_transfer_data(hw_frame, frame, 0) < 0)
			{
				log.error("Failed to transfer HW frame\n");
				break;
			}

			use_frame = hw_frame;
		}

		if (!ctx->rgb_frame ||
			ctx->last_width != use_frame->width ||
			ctx->last_height != use_frame->height)
		{
			if (ctx->rgb_frame)
				av_frame_free(&ctx->rgb_frame);

			if (ctx->sws_ctx)
				sws_freeContext(ctx->sws_ctx);

			ctx->rgb_frame = av_frame_alloc();
			ctx->rgb_frame->format = AV_PIX_FMT_BGRA;
			ctx->rgb_frame->width = use_frame->width;
			ctx->rgb_frame->height = use_frame->height;

			av_frame_get_buffer(ctx->rgb_frame, 32);

			ctx->sws_ctx = sws_getContext(
				use_frame->width, use_frame->height,
				(AVPixelFormat)use_frame->format,
				ctx->rgb_frame->width, ctx->rgb_frame->height,
				AV_PIX_FMT_BGRA,
				SWS_BILINEAR, nullptr, nullptr, nullptr);

			ctx->last_width = use_frame->width;
			ctx->last_height = use_frame->height;
		}

		sws_scale(ctx->sws_ctx,
			use_frame->data, use_frame->linesize,
			0, use_frame->height,
			ctx->rgb_frame->data, ctx->rgb_frame->linesize);

		NDIlib_video_frame_v2_t ndi_frame;
		ndi_frame.xres = ctx->rgb_frame->width;
		ndi_frame.yres = ctx->rgb_frame->height;
		ndi_frame.FourCC = NDIlib_FourCC_type_BGRA;
		ndi_frame.frame_rate_N = 240000;
		ndi_frame.frame_rate_D = 1001;
		ndi_frame.picture_aspect_ratio =
			(float)ctx->rgb_frame->width / ctx->rgb_frame->height;
		ndi_frame.timecode = NDIlib_send_timecode_synthesize;
		ndi_frame.p_data = ctx->rgb_frame->data[0];
		ndi_frame.line_stride_in_bytes =
			ctx->rgb_frame->linesize[0];

		NDIlib_send_send_video_v2(ctx->ndi_sender, &ndi_frame);

		av_frame_unref(frame);
		if (hw_frame)
			av_frame_unref(hw_frame);
	}

	av_frame_free(&frame);
	av_packet_free(&avpkt);

	if (hw_frame)
		av_frame_free(&hw_frame);

	//free(pkt->data);
}

static void on_audio_data_1(struct imf::SspAudioData* audio)
{

}

static void on_audio_data_2(struct imf::SspAudioData* audio)
{

}

static void on_meta_1(struct imf::SspVideoMeta* vmeta, struct imf::SspAudioMeta* ameta, struct imf::SspMeta* m)
{
	Logger log("zcam_native.log");

	log.info("Video Meta received: {}x{}  {}/{}, Encoder: {}\n",
		vmeta->width,
		vmeta->height,
		vmeta->unit,
		vmeta->timescale,
		vmeta->encoder);

	log.info("Audio Meta received, SampleRate: {}", ameta->sample_rate);
}

static void on_meta_2(struct imf::SspVideoMeta* v, struct imf::SspAudioMeta* a, struct imf::SspMeta* m)
{

}

static void on_disconnect()
{
	Logger log("zcam_native.log");
	log.info("on disconnect\n");
}

static std::vector<ClientInput> gClientInputs;

static void setup(imf::Loop* loop)
{
	Logger log("zcam_native.log");

	// Define client inputs (IP + decoder type)	

	const int client_count = gClientInputs.size();

	for (int i = 0; i < client_count; ++i) {
		const auto& input = gClientInputs[i];

		// Create ClientContext
		auto ctx = std::make_unique<ClientContext>();
		ctx->client_id = i;
		ctx->name = input.ndi_name;
		ctx->clientConfig.decoderType = input.decoderType;

		// Initialize decoder
		if (!init_decoder(*ctx)) {
			log.error("Decoder init failed for client %d\n", i);
			continue;
		}	

		// Initialize NDI sender
		NDIlib_send_create_t NDI_send_create_desc;
		NDI_send_create_desc.p_ndi_name = ctx->name.c_str();
		ctx->ndi_sender = NDIlib_send_create(&NDI_send_create_desc);
		if (!ctx->ndi_sender) {
			log.error("Failed to create NDI sender for client %d\n", i);
			continue;
		}

		// Save context pointer for lambda
		ClientContext* ctx_ptr = ctx.get();
	
		auto client = std::make_unique<imf::SspClient>(
			input.ip,
			loop,
			0x400000);

		client->init();

		client->setOnH264DataCallback(
			[ctx_ptr](imf::SspH264Data* h264)
			{
				uint8_t* copy = (uint8_t*)malloc(h264->len);
				memcpy(copy, h264->data, h264->len);

				VideoPacket pkt{ copy, h264->len, h264->frm_no };

				ctx_ptr->videoQueue.enqueue(
					pkt,
					h264->pts,
					h264->type == 5
				);
			}
		);
		ctx->videoQueue.setFrameCallback(
			[ctx_ptr](VideoPacket* pkt)
			{
				decode_video(ctx_ptr, pkt);
			}
		);

		client->setOnAudioDataCallback(
			[ctx_ptr](imf::SspAudioData* audio)
			{
				std::vector<uint8_t> pkt(audio->len);
				memcpy(pkt.data(), audio->data, audio->len);

				decode_audio(ctx_ptr, pkt);
			});

		client->setOnMetaCallback(on_meta_1);  // Optional: pass client ID
		client->setOnDisconnectedCallback(on_disconnect);
		client->setOnConnectionConnectedCallback([]() {
			Logger log("zcam_native.log");
			log.info("SSP connected\n");
			});
		client->setOnExceptionCallback([](int code, const char* msg) {
			Logger log("zcam_native.log");
			log.error("SSP error %d %s {} {}", code, msg);
			});


		log.info("Queue thread started for client {} ", ctx->client_id);

		client->start();
		ctx->videoQueue.start();

		// Save for lifetime management
		g_client_contexts.push_back(std::move(ctx));
		g_ssp_clients.push_back(std::move(client));

		log.info("Started client {}: {} [{}] {} {} {}", i, input.ip.c_str(),
			input.decoderType == DecoderType::HW_CUDA ? "HW_CUDA" : "SW");

		std::this_thread::sleep_for(std::chrono::seconds(5));
	}
}

void cleanup_clients() {
	for (auto& ctx : g_client_contexts) {
		if (ctx->ndi_sender)
			NDIlib_send_destroy(ctx->ndi_sender);
		if (ctx->codec_ctx)
			avcodec_free_context(&ctx->codec_ctx);
		if (ctx->hw_device_ctx)
			av_buffer_unref(&ctx->hw_device_ctx);
		if (ctx->sws_ctx)
			sws_freeContext(ctx->sws_ctx);
		if (ctx->rgb_frame)
			av_frame_free(&ctx->rgb_frame);
	}
	g_client_contexts.clear();
	g_ssp_clients.clear();
}

void handle_sigint(int) {
	running = false;
}

std::string enumToString(DecoderType type) {
	switch (type) {
	case DecoderType::SOFTWARE:
		return "SOFTWARE";
	case DecoderType::HW_CUDA:
		return "HW_CUDA";
	default:
		return "UNKNOWN";
	}
}

std::string codecToString(CodecType type) {
	switch (type) {
	case CodecType::H264:
		return "h264";
	case CodecType::HEVC:
		return "h265";
	default:
		return "UNKNOWN";
	}
}


int SetParams() {
	std::vector<std::string> found;
	Logger log("zcam_native.log");
	bool ret = false;
	std::string video_encoder;

	for (const auto& clientInput : gClientInputs)
	{
		ZCamStreamBuilder builder(clientInput.ip);
		ret = builder.set_vfr(clientInput.vfr);
		if (ret) {
			log.info("[SetParams] VFR Success for {} ", clientInput.ip);
		}else {
			log.info("[SetParams] VFR Failure for {} ", clientInput.ip);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		
		ret = builder.set_resolution(std::to_string(clientInput.width) + "x" + std::to_string(clientInput.height));
		if (ret) {
			log.info("[SetParams] Stream Resolution Success for {} ", clientInput.ip);
		}else {
			log.info("[SetParams] Stream Resolution Failure for {} ", clientInput.ip);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		if (clientInput.codecType == CodecType::HEVC) {
			video_encoder = "H.265";
		}else {
			video_encoder = "H.264";
		}
		ret = builder.set_video_encoder(video_encoder);
		if (ret) {
			log.info("[SetParams] VideoEncoder Success for {} ", clientInput.ip);
		}
		else {
			log.info("[SetParams] VideoEncoder Failure for {} ", clientInput.ip);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		ret = builder.set_stream("Stream0");
		if (ret) {
			log.info("[SetParams] set_stream 0 Success for {} ", clientInput.ip);
		}
		else {
			log.info("[SetParams] set_stream 0 Failure for {} ", clientInput.ip);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		ret = builder.set_stream("Stream1");
		if (ret) {
			log.info("[SetParams] set_stream 1 Success for {} ", clientInput.ip);
		}
		else {
			log.info("[SetParams] set_stream 1 Failure for {} ", clientInput.ip);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		ret = builder
			.index(clientInput.stream)
			.fps(clientInput.fps)
			.bitrate(clientInput.bitrate)
			.apply();

		if (ret) {
			log.info("Stream settings applied successfully for: {} ", clientInput.ip);
		}else {
			log.error("Failed to apply stream settings for: {} ", clientInput.ip);
		}
	}

	return 0;
}

int main(int argc, char** argv)
{
	if(argc < 2)
	{
		std::cout << "Usage: " << argv[0] << " <path_to_streams.json>" << std::endl;
		exit(1);
	}

	signal(SIGINT, handle_sigint);

	Logger log("zcam_native.log");

	StreamParser::ParseJson(argv[1], gClientInputs);
	if(gClientInputs.size() == 0)
	{
		log.error("No valid client inputs found in JSON. Exiting.");
		return 1;
	}

	SetParams();

	std::unique_ptr<imf::ThreadLoop> threadLooper(new imf::ThreadLoop(std::bind(setup, _1)));
	threadLooper->start();

	while (1) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	cleanup_clients();
	threadLooper->stop();
	return 0;
}
