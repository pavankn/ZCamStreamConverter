
#include "pch.h"

#include "ZcamNativeExports.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include "ClientConfig.h"
#include "ClientProcessor.h"

using json = nlohmann::json;

using namespace com_khelai_zcamnative;

ZCAM_NATIVE_API int
ZCamNative_ProcessStream(const char* streamJSON)
{
	Logger log("zcam_native.log");

	log.info("Pavankn ProcessStream called with config: {} ", streamJSON);

	std::unique_ptr<StreamParser> _streamParser = std::make_unique<StreamParser>();

	std::unique_ptr<ClientProcessor> _clientProcessor = std::make_unique<ClientProcessor>();

	std::vector<ClientConfig> _clientConfig = _streamParser->fromJson(streamJSON);

	log.info("Pavankn Num ClientConfigs Are: {} ", _clientConfig.size());

	_clientProcessor->Process(_clientConfig);

	return 0;
}