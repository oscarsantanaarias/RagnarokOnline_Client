#include "ro/core/application.hpp"
#include <windows.h>

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE /*hPrevInstance*/,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd
) {
    ro::core::Application app(hInstance, lpCmdLine ? lpCmdLine : "", nShowCmd);

    if (!app.initialize()) {
        return 1;
    }

    return app.run();
}

