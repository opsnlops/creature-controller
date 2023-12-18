#pragma once

#include <thread>

#include "logging/Logger.h"
#include "controller-config.h"


#include "e131.h"

namespace creatures {

    class E131Server {

    public:
        E131Server(const std::shared_ptr<creatures::Logger>& logger);
        ~E131Server();

        void start();


    private:

        [[noreturn]] void run();
        std::thread workerThread;
        std::shared_ptr<creatures::Logger> logger;

    };

}