#pragma once
#include <QAbstractSpinBox>
#include <QCoreApplication>
#include <QComboBox>
#include <QEvent>
#include <QObject>

namespace StreamDeck
{
//! Spin boxes and combo boxes inside a scrolled form only take the mouse
//! wheel once they have the focus: scrolling the form must not change them.
class WheelGuard final : public QObject
{
public:
  using QObject::QObject;

protected:
  bool eventFilter(QObject* o, QEvent* e) override
  {
    if(e->type() == QEvent::Wheel)
      if(auto w = qobject_cast<QWidget*>(o); w && !w->hasFocus())
      {
        if(auto parent = w->parentWidget())
          QCoreApplication::sendEvent(parent, e);
        return true;
      }
    return false;
  }
};

inline void protectFromWheel(QWidget* root)
{
  static WheelGuard guard;
  auto protect = [](QWidget* w) {
    w->setFocusPolicy(Qt::StrongFocus);
    w->installEventFilter(&guard);
  };
  for(auto w : root->findChildren<QAbstractSpinBox*>())
    protect(w);
  for(auto w : root->findChildren<QComboBox*>())
    protect(w);
}
}
