/// @file SagaLauncherWindow.h
/// @brief Declares the Product Shell launcher window and external Editor handoff.

#pragma once

#include <QMainWindow>

#include <filesystem>

namespace SagaProduct
{

class ISagaProcessLauncher;

/// Product Shell window for project discovery and external Editor launch.
class SagaLauncherWindow final : public QMainWindow
{
public:
    SagaLauncherWindow(std::filesystem::path productExecutable,
                       ISagaProcessLauncher& processLauncher,
                       QWidget* parent = nullptr);
    ~SagaLauncherWindow() override;

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace SagaProduct
