/// @file QtPanel.cpp
/// @brief Qt implementation of the QtPanel base class.

#include "SagaEditor/UI/Qt/QtPanel.h"

namespace SagaEditor
{

QtPanel::QtPanel(QWidget* parent)
    : QWidget(parent)
{}

void* QtPanel::GetNativeWidget() const noexcept
{
    return static_cast<QWidget*>(const_cast<QtPanel*>(this));
}

} // namespace SagaEditor
