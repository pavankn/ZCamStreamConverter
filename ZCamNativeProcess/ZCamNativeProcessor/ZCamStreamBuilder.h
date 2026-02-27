#pragma once
#include <iostream>
#include <curl/curl.h>
#include <map>
#include <string>

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

	ZCamStreamBuilder& bitrate(int bps) {
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

	bool apply() {
		std::string url = "http://" + ip_ + "/ctrl/stream_setting";
		bool first = true;
		for (const auto& pair : params_) {
			url += (first ? "?" : "&") + pair.first + "=" + pair.second;
			first = false;
		}

		CURL* curl = curl_easy_init();
		if (!curl) return false;

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 500L);
		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		return res == CURLE_OK;
	}


private:
	std::string ip_;
	std::map<std::string, std::string> params_;
};