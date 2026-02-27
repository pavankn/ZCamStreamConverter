#pragma once


#include <iostream>
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>
#include "Logger.h"
#include "ClientInput.h"

using json = nlohmann::json;

namespace com_khelai_zcamnative
{
	class StreamParser2
	{
	public:

		static bool ParseJson(const char* jsonPath, std::vector<ClientInput>& clientInput) {

			Logger log("zcam_native.log");

			int index = 0;

			log.info("Parsing stream config from JSON: {} ", jsonPath);

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
				if (!streamJson.contains("Resolution")) {
					log.error("Could not find Resolution");
					return false;
				}

				ClientInput _clientInput;

				if (!streamJson.contains("Ip") || !streamJson.contains("Stream") ||
					!streamJson.contains("Codec") || !streamJson.contains("HwDecoding"))
				{
					log.info("Skiiping Invalid Stream Config");
					continue;
				}
				log.info("Found Streams, Now Start Parsing");

				_clientInput.stream = streamJson["Stream"];
				_clientInput.ip = streamJson["Ip"];
				_clientInput.decoderType = streamJson["HwDecoding"] ? DecoderType::HW_CUDA : DecoderType::SOFTWARE;
				_clientInput.codecType = (streamJson["Codec"] == "H264") ? CodecType::H264 : CodecType::HEVC;
				_clientInput.ndi_name = "KHEL_NDI_" + _clientInput.ip;
				_clientInput.width = streamJson["Resolution"]["Width"];
				_clientInput.height = streamJson["Resolution"]["Height"];

				clientInput.push_back(_clientInput);
			}
			return true;
		}

	};
}

