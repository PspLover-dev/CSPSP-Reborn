#ifdef PSP
#include <pspkernel.h>
#endif

#include "App.hpp"

#include <cstdio>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    App app;
    if (!app.init()) {
        std::printf("CSPSP failed to start.\n");
        return 1;
    }
    app.run();
    app.shutdown();
    return 0;
}
