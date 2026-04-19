#pragma once

#include <set>

#include <cerrno>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

class TWorkerList {
    std::set<pid_t> workers;

public:
    void addWorker(pid_t worker) {
        workers.emplace(worker);
    }

    void waitZombiesNoBlock() {
        while(true) {
            int st = 0;
            pid_t w = waitpid(-1, &st, WNOHANG);
            if (w <= 0) {
                break;
            }
            workers.erase(w);
        }
    }

    ~TWorkerList() {
        while(true) {
            int st = 0;
            if (waitpid(-1, &st, 0) <= 0) {
                if (errno == ECHILD) {
                    break;
                }
                if (errno == EINTR) {
                    continue;
                }
                break;
            }
        }
    }
};