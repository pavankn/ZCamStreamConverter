#include <iostream>
#include "StreamParser3.h"

int main()
{

    auto configs =
        com_khelai_zcamnative::StreamParser::ParseJson("streams.json");

    for (auto& cfg : configs)
    {
        SspSession session;
        session.start(cfg);
    }
}

