
#include "pch.h"

#include "ZcamNativeExports.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include "StreamConfig.h"

using json = nlohmann::json;

using namespace com_khelai_zcamnative;

ZCAM_NATIVE_API int
ZCamNative_ProcessStream(const char* streamconfig)
{
	std::cout << "Pavankn ProcessStream called with config: " << streamconfig << std::endl;

	std::vector<StreamConfig> _streamConfig = StreamConfig::fromJson(streamconfig);

	return 1982;
}