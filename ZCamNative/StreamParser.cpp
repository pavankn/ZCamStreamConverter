#include "StreamParser3.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include "Logger.h"

using json = nlohmann::json;

namespace com_khelai_zcamnative
{

    static HWCodecType parseCodec(const std::string& codec)
    {
        if (codec == "H264")
            return HWCodecType::H264_CUVID;

        return HWCodecType::HEVC_CUVID;
    }

    std::vector<ClientConfig>
        StreamParser::ParseJson(const std::string& jsonPath)
    {
        Logger log("zcam_native.log");

        log.info("Parsing stream config from JSON: {}", jsonPath);

        std::ifstream file(jsonPath);
        if (!file)
            throw std::runtime_error("Failed to open JSON file");

        json root;
        file >> root;

        if (!root.contains("Streams") || !root["Streams"].is_array())
            throw std::runtime_error("'Streams' array missing");

        std::vector<ClientConfig> configs;

        int index = 0;

        for (const auto& s : root["Streams"])
        {
            if (!s.contains("Ip") ||
                !s.contains("Stream") ||
                !s.contains("Codec") ||
                !s.contains("HwDecoding"))
            {
                log.warn("Skipping invalid stream entry");
                continue;
            }

            ClientConfig cfg;

            cfg.ip = s["Ip"].get<std::string>();
            cfg.stream = s["Stream"].get<std::string>();

            std::string codec = s["Codec"];
            bool hw = s["HwDecoding"];

            cfg.decoder_type =
                hw ? DecoderType::HW_CUDA : DecoderType::SOFTWARE;

            cfg.hwCodecType = parseCodec(codec);

            cfg.ndi_name = "KHEL_NDI_" + std::to_string(index++);

            // Resolution (optional)
            if (s.contains("Resolution"))
            {
                auto& r = s["Resolution"];

                cfg.width = r.value("Width", 0);
                cfg.height = r.value("Height", 0);

                log.info("Resolution {}x{}", cfg.width, cfg.height);
            }

            configs.push_back(std::move(cfg));
        }

        log.info("Parsed {} streams", configs.size());

        return configs;
    }

}