#include "stats/stats_request.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 3) {
        PrintStatsUsage(argv[0]);
        return 1;
    }

    const char* fifoReq = argv[1];
    const char* fifoResp = argv[2];

    std::string json;
    if (!RequestStats(fifoReq, fifoResp, json)) {
        perror("stats request");
        return 1;
    }

    std::cout << json << std::endl;
    return 0;
}
