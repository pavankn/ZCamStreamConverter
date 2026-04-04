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
#include "ClientInput.h"
#include "Logger.h"
#include <DbgHelp.h>
#include <Windows.h>

using namespace std::placeholders;

#pragma comment(lib, "Dbghelp.lib")

#ifdef _DEBUG
#pragma comment (lib, "libsspd.lib")
#else
#pragma comment (lib, "libssp.lib")
#endif

namespace com_khelai_zcamnative
{

	std::atomic<bool> running(true);
	std::mutex out_mutex;
	imf::Loop* gLoop = nullptr;

	std::atomic<bool> g_running{ true };


	static std::vector<std::unique_ptr<ClientContext>> g_client_contexts;
	static std::vector<std::unique_ptr<imf::SspClient>> g_ssp_clients;
	static std::vector<ClientInput> gClientInputs;


	static void cleanup_clients() {
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

			g_client_contexts.clear();
			g_ssp_clients.clear();

			ctx->ndi_running = false;
			ctx->ndi_cv.notify_all();

			if (ctx->ndi_thread.joinable())
				ctx->ndi_thread.join();
		}
	}


	void ndi_sender_thread(ClientContext* ctx)
	{
		while (ctx->ndi_running)
		{
			NDIFrame frame;

			{
				std::unique_lock<std::mutex> lock(ctx->ndi_mutex);

				ctx->ndi_cv.wait(lock, [&] {
					return !ctx->ndi_queue.empty() || !ctx->ndi_running;
					});

				if (!ctx->ndi_running)
					break;

				frame = std::move(ctx->ndi_queue.front());
				ctx->ndi_queue.pop();
			}

			NDIlib_video_frame_v2_t ndi_frame{};
			ndi_frame.xres = frame.width;
			ndi_frame.yres = frame.height;
			ndi_frame.FourCC = NDIlib_FourCC_type_BGRA;

			ndi_frame.frame_rate_N = 60000;
			ndi_frame.frame_rate_D = 1001;

			ndi_frame.picture_aspect_ratio =
				(float)frame.width / frame.height;

			ndi_frame.timecode = NDIlib_send_timecode_synthesize;

			ndi_frame.p_data = frame.data.data();
			ndi_frame.line_stride_in_bytes = frame.stride;

			NDIlib_send_send_video_v2(ctx->ndi_sender, &ndi_frame);
		}
	}

	bool init_ndi(ClientContext& ctx) {
		// Initialize NDI sender
		NDIlib_send_create_t NDI_send_create_desc;
		NDI_send_create_desc.p_ndi_name = ctx.name.c_str();
		ctx.ndi_sender = NDIlib_send_create(&NDI_send_create_desc);
		if (!ctx.ndi_sender) {
			Logger::error("Failed to create NDI sender for client %d\n", ctx.client_id);
			return false;
		}
		ctx.ndi_running = true;
		ctx.ndi_thread = std::thread(ndi_sender_thread, &ctx);
		return true;
	}


	bool init_decoder(ClientContext& ctx)
	{
		DecoderType decoderType = ctx.clientConfig.decoderType;
		CodecType codecType = ctx.clientConfig.codecType;

		AVCodecID codec_id;

		if (codecType == CodecType::H264)
			codec_id = AV_CODEC_ID_H264;
		else if (codecType == CodecType::HEVC)
			codec_id = AV_CODEC_ID_HEVC;
		else
			return false;

		ctx.codec = avcodec_find_decoder(codec_id);
		ctx.wait_i_frame = true;

		if (!ctx.codec)
		{
			Logger::error("Decoder not found for client {}", ctx.client_id);
			return false;
		}

		ctx.codec_ctx = avcodec_alloc_context3(ctx.codec);

		if (!ctx.codec_ctx)
			return false;

		// OBS-style settings for low latency
		ctx.codec_ctx->thread_count = 0;
		ctx.codec_ctx->delay = 0;

		if (decoderType == DecoderType::HW_CUDA)
		{
			if (av_hwdevice_ctx_create(
				&ctx.hw_device_ctx,
				AV_HWDEVICE_TYPE_CUDA,
				nullptr,
				nullptr,
				0) < 0)
			{
				Logger::error("Failed to create CUDA HW device for client {}", ctx.client_id);
				return false;
			}

			ctx.codec_ctx->hw_device_ctx = av_buffer_ref(ctx.hw_device_ctx);

			ctx.codec_ctx->get_format =
				[](AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts)
				{
					while (*pix_fmts != AV_PIX_FMT_NONE)
					{
						if (*pix_fmts == AV_PIX_FMT_CUDA)
							return *pix_fmts;
						pix_fmts++;
					}
					return pix_fmts[0];
				};
		}

		if (avcodec_open2(ctx.codec_ctx, ctx.codec, nullptr) < 0)
		{
			Logger::error("Could not open codec for client {}", ctx.client_id);
			return false;
		}

		Logger::info("Client {} Video decoder initialized ({})",
			ctx.client_id,
			(decoderType == DecoderType::HW_CUDA ? "HW_CUDA" : "SOFTWARE"));

		// -------------------------
		// AUDIO DECODER
		// -------------------------

		ctx.audio_codec = avcodec_find_decoder(AV_CODEC_ID_AAC);

		if (!ctx.audio_codec)
		{
			Logger::error("AAC decoder not found");
			return false;
		}

		ctx.audio_codec_ctx = avcodec_alloc_context3(ctx.audio_codec);

		if (!ctx.audio_codec_ctx)
			return false;

		if (avcodec_open2(ctx.audio_codec_ctx, ctx.audio_codec, nullptr) < 0)
		{
			Logger::error("Failed to open audio decoder");
			return false;
		}

		Logger::info("Client {} Audio decoder initialized", ctx.client_id);

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
		//-----------------------------------
		// Build AVPacket (Annex-B format)
		//-----------------------------------

		AVPacket* avpkt = av_packet_alloc();

		int total = pkt->len + 4;

		av_new_packet(avpkt, total);

		avpkt->data[0] = 0;
		avpkt->data[1] = 0;
		avpkt->data[2] = 0;
		avpkt->data[3] = 1;

		memcpy(avpkt->data + 4, pkt->data.get(), pkt->len);

		// Keyframe flag (important for decoder)
		if (pkt->type == 5)
			avpkt->flags |= AV_PKT_FLAG_KEY;

		int ret = avcodec_send_packet(ctx->codec_ctx, avpkt);

		av_packet_free(&avpkt);

		if (ret < 0)
			return;

		//-----------------------------------
		// Allocate decode frames
		//-----------------------------------

		if (!ctx->decode_frame)
			ctx->decode_frame = av_frame_alloc();

		if (ctx->clientConfig.decoderType == DecoderType::HW_CUDA &&
			!ctx->hw_frame)
			ctx->hw_frame = av_frame_alloc();

		//-----------------------------------
		// Receive decoded frames
		//-----------------------------------

		while (true)
		{
			ret = avcodec_receive_frame(
				ctx->codec_ctx,
				ctx->decode_frame);

			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				break;

			if (ret < 0)
				break;

			AVFrame* use_frame = ctx->decode_frame;

			//-----------------------------------
			// CUDA transfer
			//-----------------------------------

			if (ctx->clientConfig.decoderType == DecoderType::HW_CUDA)
			{
				av_frame_unref(ctx->hw_frame);

				if (av_hwframe_transfer_data(
					ctx->hw_frame,
					ctx->decode_frame,
					0) < 0)
				{
					Logger::error("CUDA frame transfer failed");
					break;
				}

				use_frame = ctx->hw_frame;
			}

			//-----------------------------------
			// Allocate RGB frame if needed
			//-----------------------------------

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
					use_frame->width,
					use_frame->height,
					(AVPixelFormat)use_frame->format,
					ctx->rgb_frame->width,
					ctx->rgb_frame->height,
					AV_PIX_FMT_BGRA,
					SWS_FAST_BILINEAR,
					nullptr,
					nullptr,
					nullptr);

				ctx->last_width = use_frame->width;
				ctx->last_height = use_frame->height;
			}

			//-----------------------------------
			// YUV → BGRA conversion
			//-----------------------------------

			sws_scale(
				ctx->sws_ctx,
				use_frame->data,
				use_frame->linesize,
				0,
				use_frame->height,
				ctx->rgb_frame->data,
				ctx->rgb_frame->linesize);

			//-----------------------------------
			// Push frame to NDI queue
			//-----------------------------------

			NDIFrame frame;

			frame.width = ctx->rgb_frame->width;
			frame.height = ctx->rgb_frame->height;
			frame.stride = ctx->rgb_frame->linesize[0];

			size_t frame_size = frame.stride * frame.height;

			frame.data.resize(frame_size);

			memcpy(frame.data.data(),
				ctx->rgb_frame->data[0],
				frame_size);

			{
				std::lock_guard<std::mutex> lock(ctx->ndi_mutex);

				if (ctx->ndi_queue.size() > MAX_NDI_QUEUE_SIZE)
					ctx->ndi_queue.pop();

				ctx->ndi_queue.push(std::move(frame));
			}

			ctx->ndi_cv.notify_one();

			av_frame_unref(ctx->decode_frame);
		}
	}

	static void on_audio_data_1(struct imf::SspAudioData* audio)
	{

	}

	static void on_audio_data_2(struct imf::SspAudioData* audio)
	{

	}

	static void on_meta_1(struct imf::SspVideoMeta* vmeta, struct imf::SspAudioMeta* ameta, struct imf::SspMeta* m)
	{
		Logger::info("Video Meta received: {}x{}  {}/{}, Encoder: {}",
			vmeta->width,
			vmeta->height,
			vmeta->unit,
			vmeta->timescale,
			vmeta->encoder);

		Logger::info("Audio Meta received, SampleRate: {}", ameta->sample_rate);
	}

	static void on_meta_2(struct imf::SspVideoMeta* v, struct imf::SspAudioMeta* a, struct imf::SspMeta* m)
	{

	}

	static void on_disconnect()
	{
		cleanup_clients();
		Logger::info("on disconnect\n");
	}


	static void setup(imf::Loop* loop)
	{
		// Define client inputs (IP + decoder type)	

		const int client_count = gClientInputs.size();

		for (int i = 0; i < client_count; ++i) {
			const auto& input = gClientInputs[i];

			// Create ClientContext
			auto ctx = std::make_unique<ClientContext>();
			ctx->client_id = i;
			ctx->name = input.ndi_name;
			ctx->clientConfig.decoderType = input.decoderType;
			ctx->clientConfig.codecType = input.codecType;

			// Initialize decoder
			if (!init_decoder(*ctx)) {
				Logger::error("Decoder init failed for client {}", i);
				continue;
			}

			if (!init_ndi(*ctx)) {
				Logger::error("NDI init failed for client {}", i);
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
					auto buffer = std::make_unique<uint8_t[]>(h264->len);
					memcpy(buffer.get(), h264->data, h264->len);

					VideoPacket pkt{
						std::move(buffer),
						h264->len,
						h264->frm_no,
						h264->type
					};

					ctx_ptr->videoQueue.enqueue(
						std::move(pkt),
						h264->pts,
						h264->type == 5
					);
				});

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
				Logger::info("SSP connected\n");
				});
			client->setOnExceptionCallback([](int code, const char* msg) {
				Logger::error("SSP error %d %s {} {}", code, msg);
				});


			Logger::info("Queue thread started for client {} ", ctx->client_id);

			ctx->videoQueue.start();
			client->start();

			// Save for lifetime management
			g_client_contexts.push_back(std::move(ctx));
			g_ssp_clients.push_back(std::move(client));

			Logger::info("Started client {}: {} [{}]", i, input.ip.c_str(),
				input.decoderType == DecoderType::HW_CUDA ? "HW_CUDA" : "SW");
		}
	}


	void handle_sigint(int) {
		running = false;
	}


	LONG WINAPI WriteCrashDump(EXCEPTION_POINTERS* pException) {
		char fileName[MAX_PATH];
		sprintf_s(fileName, "ZCamNativeCrash_%lu.dmp", GetCurrentProcessId());

		HANDLE hFile = CreateFileA(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if (hFile != INVALID_HANDLE_VALUE) {
			MINIDUMP_EXCEPTION_INFORMATION mei;
			mei.ThreadId = GetCurrentThreadId();
			mei.ExceptionPointers = pException;
			mei.ClientPointers = FALSE;

			// MiniDumpWithIndirectlyReferencedMemory is the production "sweet spot"
			// It captures enough to see the crash without creating a multi-GB file.
			MiniDumpWriteDump(
				GetCurrentProcess(),
				GetCurrentProcessId(),
				hFile,
				MiniDumpNormal,
				&mei, NULL, NULL
			);

			CloseHandle(hFile);
		}

		// Tell Windows to proceed with the crash (and let the Parent know)
		return EXCEPTION_EXECUTE_HANDLER;
	}


	// Signal handler for clean exit
	void signalHandler(int signum) {
		g_running = false;
	}
};

using namespace com_khelai_zcamnative;


int main(int argc, char** argv)
{
	Logger::Init("zcam_native.log");

	if (argc < 3)
	{
		Logger::error("Usage: zcam_worker <ip> <ndi_name>\n");
		return 1;
	}

	// Register the filter at the very start of main()
	SetUnhandledExceptionFilter(WriteCrashDump);

	// Register signal handlers for SIGINT (Ctrl+C) and SIGTERM
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);

	std::string ip = argv[1];
	std::string ndi_name = argv[2];

	ClientInput input;
	input.ip = ip;
	input.ndi_name = ndi_name;
	input.decoderType = DecoderType::HW_CUDA;
	input.codecType = CodecType::H264;

	gClientInputs.push_back(input);

	std::unique_ptr<imf::ThreadLoop> threadLooper(
		new imf::ThreadLoop(std::bind(setup, _1)));

	Logger::info("Starting Main Loop");

	threadLooper->start();

	// This will loop as long as the C# app is alive
	// If the C# app closes the pipe or crashes, cin.peek() will fail or EOF
	while (std::cin.good() && g_running)
	{
		if (std::cin.eof()) break; // Exit if pipe closed
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	Logger::info("Pipe closed or Stop signaled. Exiting...");
	threadLooper->stop();

	return 0;
}