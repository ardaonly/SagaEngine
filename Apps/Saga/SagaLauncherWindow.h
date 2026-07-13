/// @file SagaLauncherWindow.h
/// @brief Thin Qt Widgets renderer for the Product Launcher state.

#pragma once

#include <QMainWindow>
#include <filesystem>

namespace SagaProduct
{

class SagaLauncherWindow final : public QMainWindow
{
public:
    explicit SagaLauncherWindow(std::filesystem::path productExecutable,
                                std::filesystem::path distributionReport = {},
                                QWidget* parent = nullptr);
    ~SagaLauncherWindow() override;

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace SagaProduct
