#pragma once
#include "ClientConfig.h"

class ISspSink;

class SspSession
{
public:
    SspSession();
    ~SspSession();

    bool start(const ClientConfig& config);
    void stop();

    void setSink(ISspSink* sink);

private:
    void connect();
    void reconnect();

private:
    ClientConfig config_;

    SSPClientIso* client_ = nullptr;
    VFrameQueue* queue_ = nullptr;

    ffmpeg_decode vdecoder_{};
    ffmpeg_decode adecoder_{};

    std::atomic<bool> running_{ false };

    ISspSink* sink_ = nullptr;
};