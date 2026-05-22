/// @file SagaEditorComposerQtStaticPlugins.cpp
/// @brief Static Qt platform plugin imports for SagaEditorComposer.

#include <QtPlugin>

#if defined(QT_STATIC) && defined(SAGA_EDITOR_COMPOSER_IMPORT_QT_XCB_PLUGIN)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
#endif

#if defined(QT_STATIC) && defined(SAGA_EDITOR_COMPOSER_IMPORT_QT_OFFSCREEN_PLUGIN)
Q_IMPORT_PLUGIN(QOffscreenIntegrationPlugin)
#endif
