#pragma once

#include "server/fd_resource.h"

#include <common/scanner.h>

// Returns true if there was some kind of error.
bool WorkerMain(UniqueFd clientFd, UniqueFd statsWriteFd, const TScanConfig& scanConfig);
