#pragma once

#define ZCAM_NATIVE_EXPORTS

#ifdef ZCAM_NATIVE_EXPORTS
#define ZCAM_NATIVE_API __declspec(dllexport)
#else
#define ZCAM_NATIVE_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

	// Discover devices advertising the given service, return count, fill buffer with IP strings
	ZCAM_NATIVE_API int
		ZCamNative_ProcessStream(const char* streamconfig);

#ifdef __cplusplus
}
#endif
