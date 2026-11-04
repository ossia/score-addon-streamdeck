#pragma once
#include <QColor>
#include <QToolButton>

#include <verdigris>

namespace StreamDeck
{
class ColorButton final : public QToolButton
{
  W_OBJECT(ColorButton)
public:
  explicit ColorButton(QWidget* parent = nullptr);

  QColor color() const noexcept { return m_color; }
  void setColor(const QColor& c);

  void colorChanged(QColor c) W_SIGNAL(colorChanged, c);

private:
  void pick();
  QColor m_color{Qt::black};
};
}
