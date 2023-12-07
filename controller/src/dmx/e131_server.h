#pragma once

#include <thread>

#include "controller-config.h"
#include "namespace-stuffs.h"

#include "e131.h"

namespace creatures {

    class E131Server {

    public:
        E131Server();
        ~E131Server();

        void start();


    private:

        [[noreturn]] void run();
        std::thread workerThread;

    };

}