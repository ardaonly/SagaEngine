/// @file QtUIApplication.h
/// @brief Qt backend implementation of IUIApplication — wraps QApplication.

#pragma once

#include "SagaEditor/UI/IUIApplication.h"
#include <memory>

namespace SagaEditor
{

// ─── Qt Application ───────────────────────────────────────────────────────────

/// Owns QApplication behind a pimpl so no Qt header leaks into engine code.
class QtUIApplication final : public IUIApplication
{
public:
    QtUIApplication(int& argc, char** argv);
    ~QtUIApplication() override;

    int  Run()  override;
    void Quit() override;

private:
    struct Impl;                   ///< Pimpl — hides QApplication.
    std::unique_ptr<Impl> m_impl;
};

} // namespace SagaEditor
