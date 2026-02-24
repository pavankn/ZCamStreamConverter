#include "pch.h"

#include <iostream>
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>
#include "Logger.h"

using json = nlohmann::json;


namespace com_khelai_zcamnative
{
	Logger log("zcam_native.log");

	class StreamConfig
	{
	public:
		std::string ip;
		std::string codec;
		std::string stream;
		bool hwDecoding;
		int width;
		int height;
		int frameRate;

		static std::vector<StreamConfig> fromJson(const char* jsonPath) {

			std::vector<StreamConfig> configs;

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

				StreamConfig config;
				config.ip = streamJson["Ip"];
				config.codec = streamJson["Codec"];
				config.hwDecoding = streamJson["HwDecoding"];
				config.stream = streamJson["Stream"];

				log.info("Parsed stream config - IP: {}, Codec: {}, HW Decoding: {}, Stream: {} ",
					config.ip, config.codec, config.hwDecoding, config.stream);

				if (!streamJson.contains("Resolution")) {
					log.info("Could not find Resolution");
				}
				else {
					config.width = streamJson["Resolution"]["Width"];
					config.height = streamJson["Resolution"]["Height"];
					log.info("Found Resolution: Width: {}, Height: {} ", config.width, config.height);
				}							

				configs.push_back(config);
			}
			return configs;
		}
	};
}

