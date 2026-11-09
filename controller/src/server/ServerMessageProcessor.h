
#pragma once

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "creature/logging/Logger.h"
#include "creature/thread/ThreadName.h"
#include "util/Result.h"
#include "util/StoppableThread.h"

namespace creatures::server {

class ServerMessageProcessor : public StoppableThread {};

} // namespace creatures::server