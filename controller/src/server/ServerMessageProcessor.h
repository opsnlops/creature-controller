
#pragma once

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "logging/Logger.h"
#include "util/Result.h"
#include "util/StoppableThread.h"
#include "util/thread_name.h"

namespace creatures ::server {

class ServerMessageProcessor : public StoppableThread {};

} // namespace creatures::server