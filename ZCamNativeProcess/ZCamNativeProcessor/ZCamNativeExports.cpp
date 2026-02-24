
#include "pch.h"

#include "ZcamNativeExports.h"
#include <iostream>

ZCAM_NATIVE_API int
ZCamNative_ProcessStream(const char* streamconfig)
{
	std::cout << "Pavankn ProcessStream called with config: " << streamconfig << std::endl;
	return 1982;
}