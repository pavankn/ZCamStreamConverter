#include "ClientProcessor.h"
#include "Logger.h"

using namespace std::placeholders;

#ifdef _DEBUG
#pragma comment (lib, "libsspd.lib")
#else
#pragma comment (lib, "libssp.lib")
#endif

static std::vector<ClientConfig> g_ClientConfigs;

namespace com_khelai_zcamnative
{  
	Logger log("zcam_native.log");

	static void on_audio_data(struct imf::SspAudioData* audio)
	{
	}

	static void on_metadata(struct imf::SspVideoMeta* v, struct imf::SspAudioMeta* a, struct imf::SspMeta* m)
	{
		printf("on meta 1 wall clock %d", m->pts_is_wall_clock);
		printf("              video %dx%d %d/%d\n", v->width, v->height, v->unit, v->timescale);
		printf("              audio %d\n", a->sample_rate);
	}

	static void on_disconnect()
	{
		printf("on disconnect\n");
	}

	void ClientProcessor::Stop()
	{
		if (threadLoop_) {
			threadLoop_->stop();
			threadLoop_.reset();
		}
	}
	void ClientProcessor::Setup(imf::Loop* loop)
	{
		log.info("Pavankn Inside Setup with loop: num ClientConfigs: {} ", g_ClientConfigs.size());

		log.info("Printing Loop Now");
		log.info("Printed Loop");

		int index = 0;
		for(auto client_config : g_ClientConfigs)
		{
			ClientContext ctx;
			ctx.client_id = index;
			ctx.name = client_config.ndi_name;
			ctx.type = client_config.decoder_type;
			ctx.hwCodecType = client_config.hwCodecType;

			log.info("Pavankn Landed Here");

			if(!init_decoder(ctx)) {
				log.error("Failed to initialize decoder for client ", ctx.client_id);
				continue;
			}

			log.info("Decoder initialized for client {} with type {} and codec {} ", index,
				ctx.type == DecoderType::HW_CUDA ? "HW_CUDA" : "SOFTWARE",
				ctx.hwCodecType == HWCodecType::H264_CUVID ? "H264_CUVID" : "HEVC_CUVID");

			log.info("Initializing NDI");

			// Initialize NDI sender
			NDIlib_send_create_t NDI_send_create_desc;
			NDI_send_create_desc.p_ndi_name = ctx.name.c_str();
			ctx.ndi_sender = NDIlib_send_create(&NDI_send_create_desc);
			if (!ctx.ndi_sender) {
				log.error("Failed to create NDI sender for client ", index);
				continue;
			}

			log.info("Initialized NDI, IP= {}", client_config.ip);

			// Create SspClient
			auto client = std::make_unique<imf::SspClient>(client_config.ip, loop, 0x400000);
			log.info("Before Client Init ");
			client->init();
			log.info("Initialized SspClient for client {} ", index);
			client->setOnH264DataCallback(std::bind(&ClientProcessor::handle_video_data, this, std::ref(ctx), _1));
			log.info("setOnH2646DataCallback for client {} ", index);
			client->setOnMetaCallback(on_metadata);
			log.info("setOnMetaCallback for client {} ", index);
			client->setOnAudioDataCallback(on_audio_data);
			client->setOnDisconnectedCallback(on_disconnect);

			log.info("Starting client for {} ", client_config.ip, " with NDI name {} ", ctx.name);

			client->start();
		}
	}

	void ClientProcessor::cleanup_client_context_cuda(ClientContext& ctx)
	{
		/*if (ctx.d_bgra_frame) {
			CHECK_CUDA(cudaFree(ctx.d_bgra_frame));
			ctx.d_bgra_frame = nullptr;
		}*/
		ctx.cuda_resources_initialized = false;
		// Also free AVFrame and SwsContext if they are kept for SW path
		if (ctx.rgb_frame) {
			av_frame_free(&ctx.rgb_frame);
			ctx.rgb_frame = nullptr;
		}
		if (ctx.sws_ctx) {
			sws_freeContext(ctx.sws_ctx);
			ctx.sws_ctx = nullptr;
		}
	}

	void ClientProcessor::handle_video_data(ClientContext& ctx, imf::SspH264Data* h264)
	{
		std::lock_guard<std::mutex> lock(ctx.decoder_mutex);

		if (!ctx.codec_ctx) return;

		AVPacket* pkt = av_packet_alloc();
		pkt->data = h264->data;
		pkt->size = h264->len;

		if (avcodec_send_packet(ctx.codec_ctx, pkt) < 0) {
			av_packet_free(&pkt);
			return;
		}

		AVFrame* frame = av_frame_alloc();
		AVFrame* hw_frame = nullptr;

		while (true) {
			int ret = avcodec_receive_frame(ctx.codec_ctx, frame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				break;
			if (ret < 0) break;

			AVFrame* use_frame = frame;
			printf("Decoded frame %d size %dx%d\n", h264->frm_no, frame->width, frame->height);

			// If hardware decode, transfer to system memory
			if (ctx.type == DecoderType::HW_CUDA) {
				if (!hw_frame) hw_frame = av_frame_alloc();
				if (av_hwframe_transfer_data(hw_frame, frame, 0) < 0) {
					printf("Failed to transfer HW frame to CPU\n");
					break;
				}
				use_frame = hw_frame;
			}

			// Prepare or reallocate RGB frame
			if (!ctx.rgb_frame || ctx.last_width != use_frame->width || ctx.last_height != use_frame->height) {
				if (ctx.rgb_frame) av_frame_free(&ctx.rgb_frame);
				if (ctx.sws_ctx) sws_freeContext(ctx.sws_ctx);

				ctx.rgb_frame = av_frame_alloc();
				ctx.rgb_frame->format = AV_PIX_FMT_BGRA;
				ctx.rgb_frame->width = use_frame->width;
				ctx.rgb_frame->height = use_frame->height;
				av_frame_get_buffer(ctx.rgb_frame, 32);

				ctx.sws_ctx = sws_getContext(
					use_frame->width, use_frame->height,
					(AVPixelFormat)use_frame->format,
					ctx.rgb_frame->width, ctx.rgb_frame->height,
					AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr);

				if (!ctx.sws_ctx) {
					printf("sws_getContext failed for client %d\n", ctx.client_id);
					break;
				}

				ctx.last_width = use_frame->width;
				ctx.last_height = use_frame->height;
			}

			sws_scale(ctx.sws_ctx,
				use_frame->data, use_frame->linesize,
				0, use_frame->height,
				ctx.rgb_frame->data, ctx.rgb_frame->linesize);

			NDIlib_video_frame_v2_t ndi_frame;
			ndi_frame.xres = ctx.rgb_frame->width;
			ndi_frame.yres = ctx.rgb_frame->height;
			ndi_frame.FourCC = NDIlib_FourCC_type_BGRA;
			ndi_frame.frame_rate_N = 240000;
			ndi_frame.frame_rate_D = 1001;
			ndi_frame.picture_aspect_ratio = (float)ctx.rgb_frame->width / ctx.rgb_frame->height;
			ndi_frame.timecode = NDIlib_send_timecode_synthesize;
			ndi_frame.p_data = ctx.rgb_frame->data[0];
			ndi_frame.line_stride_in_bytes = ctx.rgb_frame->linesize[0];

			NDIlib_send_send_video_v2(ctx.ndi_sender, &ndi_frame);

			av_frame_unref(frame);
			if (hw_frame) av_frame_unref(hw_frame);
		}

		if (hw_frame) av_frame_free(&hw_frame);
		av_frame_free(&frame);
		av_packet_free(&pkt);
	}

	bool ClientProcessor::init_decoder(ClientContext& ctx)
	{
		std::string decoderName;
		log.info("Initializing decoder in Progress for client ", ctx.client_id, " with type ", 
			(ctx.type == DecoderType::HW_CUDA ? "HW_CUDA" : "SOFTWARE"), 
			" and codec ", (ctx.hwCodecType == HWCodecType::H264_CUVID ? "H264_CUVID" : "HEVC_CUVID"));

		if (ctx.type == DecoderType::HW_CUDA) {
			if (ctx.hwCodecType == HWCodecType::H264_CUVID) {
				log.info("Initializing CUDA decoder for client ", ctx.client_id, " with H264_CUVID");
				decoderName = "h264_cuvid";
			}
			else if (ctx.hwCodecType == HWCodecType::HEVC_CUVID) {
				log.info("Initializing CUDA decoder for client ", ctx.client_id, " with HEVC_CUVID");
				decoderName = "hevc_cuvid";
			}
			else {
				return false;
			}
			ctx.codec = avcodec_find_decoder_by_name(decoderName.c_str());  // or "h264_nvdec"
			if (!ctx.codec) {
				printf("Client[%d] CUDA decoder not found\n", ctx.client_id);
				return false;
			}

			ctx.codec_ctx = avcodec_alloc_context3(ctx.codec);
			if (!ctx.codec_ctx) return false;

			if (av_hwdevice_ctx_create(&ctx.hw_device_ctx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0) < 0) {
				printf("Failed to create CUDA HW device for client %d\n", ctx.client_id);
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
			if (ctx.hwCodecType == HWCodecType::H264_CUVID) {
				ctx.codec = avcodec_find_decoder(AV_CODEC_ID_H264);
			}
			else if (ctx.hwCodecType == HWCodecType::HEVC_CUVID) {
				ctx.codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
			}
			else {
				return false;
			}

			if (!ctx.codec) {
				printf("Client[%d] software decoder not found\n", ctx.client_id);
				return false;
			}

			ctx.codec_ctx = avcodec_alloc_context3(ctx.codec);
			if (!ctx.codec_ctx) return false;
		}

		if (avcodec_open2(ctx.codec_ctx, ctx.codec, nullptr) < 0) {
			printf("Could not open codec for client %d\n", ctx.client_id);
			return false;
		}

		printf("Codec pixel format: %s\n", av_get_pix_fmt_name(ctx.codec_ctx->pix_fmt));

		printf("Client[%d] decoder (%s) initialized.\n", ctx.client_id,
			(ctx.type == DecoderType::HW_CUDA ? "HW_CUDA" : "SOFTWARE"));
		return true;
	}

    int ClientProcessor::Process(std::vector<ClientConfig>& clientConfig) {

		if (threadLoop_) {
			log.error("ClientProcessor is already running. Stop it before starting a new one.");
			return 0; // already running
		}

		log.info("Number of Clients Detected {} ", clientConfig.size());

		for(auto config : clientConfig)
		{
			m_ClientConfigs.push_back(config);
			g_ClientConfigs.push_back(config); // Store in global variable for access in static callbacks
		}

		threadLoop_ = std::make_unique<imf::ThreadLoop>(
			std::bind(&ClientProcessor::Setup, this, std::placeholders::_1)
		);

		log.info("Starting thread loop for ClientProcessor");

		threadLoop_->start();

		while (1) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}

		return 0; // MUST return to C#
	}	
}

