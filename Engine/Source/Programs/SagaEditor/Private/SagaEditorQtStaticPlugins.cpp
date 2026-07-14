/// @file SagaEditorQtStaticPlugins.cpp
/// @brief Static Qt platform plugin imports for the standalone SagaEditor executable.

#include <QtPlugin>

#if defined(QT_STATIC) && defined(SAGA_EDITOR_IMPORT_QT_XCB_PLUGIN)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
#endif

#if defined(QT_STATIC) && defined(SAGA_EDITOR_IMPORT_QT_OFFSCREEN_PLUGIN)
Q_IMPORT_PLUGIN(QOffscreenIntegrationPlugin)
#endif
