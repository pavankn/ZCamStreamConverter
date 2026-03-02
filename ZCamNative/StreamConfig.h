#pragma once

#pragma once
#include <string>

struct StreamConfig
{
    std::string ip;
    std::string stream;
    std::string codec;

    bool hwDecoding = false;

    int width = 0;
    int height = 0;
};