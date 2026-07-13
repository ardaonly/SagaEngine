/// @file EditorApplicationRunner.h
/// @brief Declares the Editor-owned command-line and application runner.

#pragma once

#include <functional>
#include <iosfwd>

namespace SagaEditor
{

class IUIFactory;
class EditorShell;

/// Optional development-only hook invoked after the Editor shell initializes.
using EditorShellExtension = std::function<void(EditorShell&)>;

/// Parse Editor arguments and delegate to inspection or UI application services.
[[nodiscard]] int RunEditorApplication(int argc,
                                       char* argv[],
                                       IUIFactory& factory,
                                       std::ostream& out,
                                       std::ostream& err,
                                       EditorShellExtension extension = {});

/// Current bounded command-line help text for the SagaEditor host.
[[nodiscard]] const char* EditorApplicationUsage() noexcept;

} // namespace SagaEditor
