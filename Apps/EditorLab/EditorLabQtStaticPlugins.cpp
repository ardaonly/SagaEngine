/// @file EditorLabQtStaticPlugins.cpp
/// @brief Static Qt platform plugin imports for the standalone EditorLab executable.

#include <QtPlugin>

#if defined(QT_STATIC)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
#endif
