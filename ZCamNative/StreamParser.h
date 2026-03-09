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
	class StreamParser
	{
	private:
		static std::string getLastIpPart(const std::string& ip)
		{
			auto pos = ip.find_last_of('.');
			if (pos == std::string::npos || pos + 1 >= ip.size())
				throw std::runtime_error("Invalid IP address");

			return ip.substr(pos + 1);
		}
		static CodecType parseCodec(std::string codec)
		{
			std::transform(codec.begin(), codec.end(),
				codec.begin(), ::toupper);

			if (codec == "H264")
				return CodecType::H264;

			return CodecType::HEVC;
		}
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
					continue;
				}

				ClientInput _clientInput;

				if (!streamJson.contains("Ip") || !streamJson.contains("Codec"))
				{
					log.info("Skiiping Invalid Stream Config");
					continue;
				}
				log.info("Found Streams, Now Start Parsing");

				
				_clientInput.ip = streamJson["Ip"].get<std::string>();

				bool hw = streamJson["HwDecoding"].get<bool>();
				_clientInput.decoderType = hw ? DecoderType::HW_CUDA : DecoderType::SOFTWARE;

				_clientInput.codecType = parseCodec(streamJson["Codec"].get<std::string>());

				if (_clientInput.codecType == CodecType::HEVC) {
					_clientInput.stream = "stream0";
				}else {
					_clientInput.stream = "stream1";
				}
								
				_clientInput.ndi_name = std::string("KHEL_NDI_") + getLastIpPart(_clientInput.ip) ;
				_clientInput.width = streamJson["Resolution"]["Width"];
				_clientInput.height = streamJson["Resolution"]["Height"];
				_clientInput.fps = streamJson.value("Fps", 0);
				_clientInput.vfr = streamJson.value("VFR", 0);
				_clientInput.bitrate = streamJson.value("Bitrate", 0);

				_clientInput.bitrate *= 1024 * 1024;

				clientInput.push_back(_clientInput);
				index++;
			}
			return true;
		}
	};
}

