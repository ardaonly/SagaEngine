/// @file SagaQtStaticPlugins.cpp
/// @brief Static Qt platform plugin imports for the primary Saga executable.

#include <QtPlugin>

#if defined(QT_STATIC)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
#endif
