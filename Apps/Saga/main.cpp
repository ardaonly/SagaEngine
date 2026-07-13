/// @file main.cpp
/// @brief Saga product orchestration entry point.

#include "SagaApp.h"
#include "SagaAppConfig.h"

#include <iostream>

int main(int argc, char* argv[])
{
    const SagaProduct::SagaConfigResult parsed =
        SagaProduct::ParseSagaAppConfig(argc, argv);
    if (!parsed.ok)
    {
        std::cerr << parsed.error << '\n';
        return 2;
    }
    if (parsed.config.showHelp)
    {
        std::cout << SagaProduct::SagaUsageText();
        return 0;
    }

    SagaProduct::SagaApp app;
    return app.Run(argc, argv, parsed.config, std::cout, std::cerr);
}
