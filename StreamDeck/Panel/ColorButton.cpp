#include "ColorButton.hpp"

#include <QColorDialog>
#include <QPainter>
#include <QPixmap>

#include <wobjectimpl.h>
W_OBJECT_IMPL(StreamDeck::ColorButton)

namespace StreamDeck
{
ColorButton::ColorButton(QWidget* parent)
    : QToolButton{parent}
{
  setIconSize({36, 16});
  setToolButtonStyle(Qt::ToolButtonIconOnly);
  connect(this, &QToolButton::clicked, this, &ColorButton::pick);
  setColor(m_color);
}

void ColorButton::setColor(const QColor& c)
{
  m_color = c;
  QPixmap pix{iconSize()};
  pix.fill(Qt::transparent);
  {
    QPainter p{&pix};
    p.fillRect(pix.rect(), QColor{0x80, 0x80, 0x80});
    p.fillRect(pix.rect().adjusted(1, 1, -1, -1), c);
  }
  setIcon(QIcon{pix});
  setToolTip(c.name(QColor::HexArgb));
}

void ColorButton::pick()
{
  const auto c = QColorDialog::getColor(
      m_color, this, tr("Colour"), QColorDialog::ShowAlphaChannel);
  if(c.isValid() && c != m_color)
  {
    setColor(c);
    colorChanged(c);
  }
}
}
