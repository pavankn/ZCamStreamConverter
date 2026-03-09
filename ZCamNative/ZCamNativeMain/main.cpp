#define _GLIBCXX_USE_CXX11_ABI 1
#include <functional>
#include <memory>
#include <thread>

#include <stdlib.h>
#include <iostream>
#include <mutex>
#include <vector>
#include <string>
#include <curl/curl.h>
#include <algorithm>  // for std::min
#include <map>

#include <windows.h>
#include <signal.h>
#include "StreamParser.h"
#include "imf/net/loop.h"
#include "imf/net/threadloop.h"
#include "imf/ssp/sspclient.h"

std::atomic<bool> running(true);

static std::vector<std::unique_ptr<ClientContext>> g_client_contexts;
static std::vector<std::unique_ptr<imf::SspClient>> g_ssp_clients;
static std::vector<ClientInput> gClientInputs;

using namespace com_khelai_zcamnative;

struct WorkerProcess {
	std::string ip;
	HANDLE hProcess;
};

std::vector<WorkerProcess> gActiveWorkers;

static size_t WriteCallback(void* contents, size_t size,
	size_t nmemb, void* userp)
{
	((std::string*)userp)->append(
		(char*)contents, size * nmemb);
	return size * nmemb;
}

class ZCamStreamBuilder {
public:
	ZCamStreamBuilder(const std::string& ip)
		: ip_(ip) {
		params_["index"] = "stream1"; // default
	}

	ZCamStreamBuilder& index(const std::string& index) {
		params_["index"] = index;
		return *this;
	}

	ZCamStreamBuilder& resolution(int w, int h) {
		params_["width"] = std::to_string(w);
		params_["height"] = std::to_string(h);
		return *this;
	}

	ZCamStreamBuilder& bitrate(uint32_t bps) {
		params_["bitrate"] = std::to_string(bps);
		return *this;
	}

	ZCamStreamBuilder& encoder(const std::string& enc) {
		params_["venc"] = enc;
		return *this;
	}

	ZCamStreamBuilder& fps(int value) {
		params_["fps"] = std::to_string(value);
		return *this;
	}

	bool apply()
	{
		Logger log("zcam_native.log");

		CURL* curl = curl_easy_init();
		if (!curl) return false;

		std::string response;

		std::string url =
			"http://" + ip_ + "/ctrl/stream_setting";

		bool first = true;
		for (const auto& pair : params_)
		{
			char* enc =
				curl_easy_escape(curl,
					pair.second.c_str(), 0);

			url += (first ? "?" : "&") +
				pair.first + "=" + enc;

			curl_free(enc);
			first = false;
		}

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");

		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		if (res != CURLE_OK)
			return false;

		// SUCCESS
		if (response.find("\"code\":0") != std::string::npos) {
			return true;
		}

		return false;
	}
	bool set_vfr(int vfr)
	{
		CURL* curl = curl_easy_init();
		if (!curl) return false;

		std::string response;

		std::string url =
			"http://" + ip_ + "/ctrl/set?movvfr=" + std::to_string(vfr);

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		return (res == CURLE_OK &&
			response.find("\"code\":0") != std::string::npos);
	}
	bool set_resolution(std::string resolution)
	{
		CURL* curl = curl_easy_init();
		if (!curl) return false;

		std::string response;

		std::string url =
			"http://" + ip_ + "/ctrl/set?resolution=" + resolution;

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		return (res == CURLE_OK &&
			response.find("\"code\":0") != std::string::npos);
	}
	bool set_video_encoder(std::string videoencoder)
	{
		CURL* curl = curl_easy_init();
		if (!curl) return false;

		std::string response;

		std::string url =
			"http://" + ip_ + "/ctrl/set?video_encoder=" + videoencoder;

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		return (res == CURLE_OK &&
			response.find("\"code\":0") != std::string::npos);
	}

	bool set_stream(std::string streamindex)
	{
		CURL* curl = curl_easy_init();
		if (!curl) return false;

		std::string response;

		std::string url =
			"http://" + ip_ + "/ctrl/set?send_stream=" + streamindex;

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		return (res == CURLE_OK &&
			response.find("\"code\":0") != std::string::npos);
	}

private:
	std::string ip_;
	std::map<std::string, std::string> params_;
};


HANDLE spawn_worker(const ClientInput& input, const std::string workerPath)
{	
	// CreateProcess needs a modifiable char buffer
	std::vector<char> cmdLine(workerPath.begin(), workerPath.end());
	cmdLine.push_back('\0');

	STARTUPINFOA si = { sizeof(si) };
	PROCESS_INFORMATION pi = { 0 };

	BOOL ok = CreateProcessA(
		NULL, cmdLine.data(), NULL, NULL, FALSE,
		CREATE_NEW_CONSOLE, // Useful for debugging; change to 0 for background
		NULL, NULL, &si, &pi
	);

	if (!ok) {
		fprintf(stderr, "Failed to spawn worker for %s. Error: %lu\n",
			input.ip.c_str(), GetLastError());
		return NULL;
	}

	// We don't need the thread handle, but we MUST keep the process handle to monitor it
	CloseHandle(pi.hThread);
	return pi.hProcess;
}

void handle_sigint(int) {
	running = false;
}

int SetParams() {
	std::vector<std::string> found;
	Logger log("zcam_native.log");
	bool ret = false;
	std::string video_encoder;

	for (const auto& clientInput : gClientInputs)
	{
		ZCamStreamBuilder builder(clientInput.ip);
		ret = builder.set_vfr(clientInput.vfr);
		if (ret) {
			log.info("[SetParams] VFR Success for {} ", clientInput.ip);
		}
		else {
			log.info("[SetParams] VFR Failure for {} ", clientInput.ip);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		ret = builder.set_resolution(std::to_string(clientInput.width) + "x" + std::to_string(clientInput.height));
		if (ret) {
			log.info("[SetParams] Stream Resolution Success for {} ", clientInput.ip);
		}
		else {
			log.info("[SetParams] Stream Resolution Failure for {} ", clientInput.ip);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		if (clientInput.codecType == CodecType::HEVC) {
			video_encoder = "H.265";
		}
		else {
			video_encoder = "H.264";
		}
		ret = builder.set_video_encoder(video_encoder);
		if (ret) {
			log.info("[SetParams] VideoEncoder Success for {} ", clientInput.ip);
		}
		else {
			log.info("[SetParams] VideoEncoder Failure for {} ", clientInput.ip);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		ret = builder.set_stream("Stream0");
		if (ret) {
			log.info("[SetParams] set_stream 0 Success for {} ", clientInput.ip);
		}
		else {
			log.info("[SetParams] set_stream 0 Failure for {} ", clientInput.ip);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		ret = builder.set_stream("Stream1");
		if (ret) {
			log.info("[SetParams] set_stream 1 Success for {} ", clientInput.ip);
		}
		else {
			log.info("[SetParams] set_stream 1 Failure for {} ", clientInput.ip);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		ret = builder
			.index(clientInput.stream)
			.fps(clientInput.fps)
			.bitrate(clientInput.bitrate)
			.apply();

		if (ret) {
			log.info("Stream settings applied successfully for: {} ", clientInput.ip);
		}
		else {
			log.error("Failed to apply stream settings for: {} ", clientInput.ip);
		}
	}

	return 0;
}


int main(int argc, char** argv)
{
	if (argc < 3)
	{
		std::cout << "Usage: " << argv[0]
			<< " <path to worker exe> <path_to_streams.json>" << std::endl;
		return 1;
	}

	signal(SIGINT, handle_sigint);

	Logger log("zcam_native.log");

	std::string workerPath(argv[1]);

	StreamParser::ParseJson(argv[1], gClientInputs);

	if (gClientInputs.empty())
	{
		log.error("No valid client inputs found.");
		return 1;
	}

	SetParams();

	log.info("Spawning camera workers...");

	// 1. Initial Spawn
	std::vector<HANDLE> hProcesses;
	std::vector<ClientInput> activeInputs;

	for (const auto& input : gClientInputs) {
		HANDLE h = spawn_worker(input, workerPath);
		if (h) {
			hProcesses.push_back(h);
			activeInputs.push_back(input);
		}
	}

	log.info("Monitoring " + std::to_string(hProcesses.size()) + " workers...");

	// 2. The Event-Driven Wait Loop
	while (!hProcesses.empty()) {
		// This blocks the thread with 0% CPU until a process exits
		DWORD result = WaitForMultipleObjects(
			(DWORD)hProcesses.size(),
			hProcesses.data(),
			FALSE,    // Wake up when ANY process dies
			INFINITE  // No timeout
		);

		// Calculate which index signaled
		DWORD index = result - WAIT_OBJECT_0;

		if (index >= 0 && index < hProcesses.size()) {
			HANDLE failedHandle = hProcesses[index];
			ClientInput failedInput = activeInputs[index];

			DWORD exitCode = 0;
			GetExitCodeProcess(failedHandle, &exitCode);

			log.error("Worker CRASHED/EXITED: " + failedInput.ip + " (Code: " + std::to_string(exitCode) + ")");

			// Cleanup the old handle
			CloseHandle(failedHandle);

			// --- RESTART LOGIC ---
			//log.info("Attempting to restart worker for " + failedInput.ip);
			//HANDLE newHandle = spawn_worker(failedInput);

			//if (newHandle) {
			//	hProcesses[index] = newHandle; // Replace the old handle with the new one
			//	// activeInputs[index] stays the same
			//}
			//else {
			//	// If restart fails, remove from monitoring list
			//	hProcesses.erase(hProcesses.begin() + index);
			//	activeInputs.erase(activeInputs.begin() + index);
			//}
		}
		else {
			log.error("Wait failed or encountered an error.");
			break;
		}
	}

	return 0;
}