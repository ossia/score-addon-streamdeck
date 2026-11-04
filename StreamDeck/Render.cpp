#include "Render.hpp"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>

namespace StreamDeck
{
const QImage& ImageCache::get(const QString& path)
{
  auto it = m_images.find(path);
  if(it == m_images.end())
  {
    if(m_images.size() > 256)
      m_images.clear();
    it = m_images.insert(path, path.isEmpty() ? QImage{} : QImage{path});
  }
  return *it;
}

QImage renderStyle(const Style& s, QSize size, ImageCache& cache)
{
  QImage img{size, QImage::Format_ARGB32_Premultiplied};
  img.fill(s.background);

  QPainter p{&img};
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);

  if(const auto& pic = cache.get(s.image); !pic.isNull())
  {
    const auto scaled
        = pic.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    p.drawImage(
        (size.width() - scaled.width()) / 2, (size.height() - scaled.height()) / 2,
        scaled);
  }

  if(!s.text.isEmpty())
  {
    const int margin = std::max(2, size.width() / 16);
    const QRect area = img.rect().adjusted(margin, margin, -margin, -margin);
    int flags = Qt::AlignHCenter | Qt::TextWordWrap;
    switch(s.align)
    {
      case Align::Top:
        flags |= Qt::AlignTop;
        break;
      case Align::Bottom:
        flags |= Qt::AlignBottom;
        break;
      case Align::Center:
        flags |= Qt::AlignVCenter;
        break;
    }

    QFont font;
    font.setBold(true);
    int px = s.textSize > 0 ? s.textSize : std::max(8, size.height() * 3 / 10);
    font.setPixelSize(px);
    if(s.textSize <= 0)
    {
      while(px > 8)
      {
        const auto r = QFontMetrics{font}.boundingRect(area, flags, s.text);
        if(r.width() <= area.width() && r.height() <= area.height())
          break;
        font.setPixelSize(--px);
      }
    }
    p.setFont(font);

    if(!cache.get(s.image).isNull())
    {
      p.setPen(QColor{0, 0, 0, 160});
      p.drawText(area.translated(1, 1), flags, s.text);
    }
    p.setPen(s.textColor);
    p.drawText(area, flags, s.text);
  }

  return img;
}
}
