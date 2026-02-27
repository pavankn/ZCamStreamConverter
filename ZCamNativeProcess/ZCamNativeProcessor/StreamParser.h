#include "pch.h"

#include <iostream>
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>
#include "Logger.h"
#include "ClientConfig.h"

using json = nlohmann::json;


namespace com_khelai_zcamnative
{
	class StreamParser
	{
	public:
		std::string ip;
		std::string codec;
		std::string stream;
		bool hwDecoding;
		int width;
		int height;
		int frameRate;
		DecoderType decoderType;
		HWCodecType hwCodecType;
		ClientConfig clientConfig;

		std::vector<ClientConfig> fromJson(const char* jsonPath) {

			Logger log("zcam_native.log");

			std::vector<ClientConfig> configs;
			int index = 0;

			log.info("Parsing stream config from JSON: ", jsonPath);

			if (!jsonPath)
				throw std::invalid_argument("jsonPath is null");

			std::ifstream file(jsonPath);
			if (!file.is_open())
				throw std::runtime_error("Failed to open streams.json");

			log.info("Opened JSON File Successfully");

			json root;
			file >> root;

			// Basic validation
			if (!root.contains("Streams") || !root["Streams"].is_array())
				throw std::runtime_error("Invalid JSON: 'Streams' array missing");

			for (const auto& streamJson : root["Streams"])
			{
				if (!streamJson.contains("Ip") || !streamJson.contains("Stream") ||
					!streamJson.contains("Codec") || !streamJson.contains("HwDecoding"))
				{
					log.info("Skiiping Invalid Stream Config");
					continue;
				}
				log.info("Found Streams, Now Start Parsing");

				StreamParser config;
				config.ip = streamJson["Ip"];
				config.codec = streamJson["Codec"];
				config.hwDecoding = streamJson["HwDecoding"];
				config.stream = streamJson["Stream"];

				clientConfig.ip = config.ip;
				clientConfig.decoder_type = config.hwDecoding ? DecoderType::HW_CUDA : DecoderType::SOFTWARE;
				clientConfig.hwCodecType = (config.codec == "H264") ? HWCodecType::H264_CUVID : HWCodecType::HEVC_CUVID;
				clientConfig.ndi_name = "KHEL_NDI_" + std::to_string(index++);
				clientConfig.stream = config.stream;

				log.info("Parsed stream config - IP: {}, Codec: {}, HW Decoding: {}, Stream: {} ",
					config.ip, config.codec, config.hwDecoding, config.stream);

				if (!streamJson.contains("Resolution")) {
					log.info("Could not find Resolution");
				}
				else {
					config.width = streamJson["Resolution"]["Width"];
					config.height = streamJson["Resolution"]["Height"];
					log.info("Found Resolution: Width: {}, Height: {} ", config.width, config.height);
					clientConfig.width = config.width;
					clientConfig.height = config.height;
				}							

				configs.push_back(clientConfig);
			}
			return configs;
		}
	};
}

