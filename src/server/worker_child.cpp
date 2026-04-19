#include "server/worker_child.h"

#include "common/message.h"

#include <pthread.h>
#include <signal.h>
#include <unistd.h>

#include <map>
#include <string>
#include "fd_resource.h"


bool WorkerMain(UniqueFd clientFd, UniqueFd statsWriteFd, const TScanConfig& scanConfig) {
    sigset_t empty;
    sigemptyset(&empty);
    pthread_sigmask(SIG_SETMASK, &empty, nullptr);
    signal(SIGPIPE, SIG_IGN);

    std::string content;
    if (!TMessage::ReadFromSocket(clientFd.get(), content)) {
        return true;
    }
    const std::map<std::string, std::uint64_t> counts = ScanContent(content, scanConfig);
    const std::string json = CountsToJsonString(counts);
    if (!TMessage::WriteToSocket(clientFd.get(), json)) {
        return true;
    }

    const std::string line = json + "\n";
    const char* p = line.data();
    std::size_t left = line.size();
    while (left > 0) {
        const ssize_t n = write(statsWriteFd.get(), p, left);
        if (n <= 0) {
            break;
        }
        p += static_cast<std::size_t>(n);
        left -= static_cast<std::size_t>(n);
    }
    return false;
}
