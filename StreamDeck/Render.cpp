#include "Render.hpp"

#include <QBuffer>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QTransform>

namespace StreamDeck
{
namespace
{
int gapFor(const Model& m)
{
  return m.keyWidth / 4;
}
}

int faceCount(const Model& m) noexcept
{
  if(!m.hasDisplay())
    return 0;
  return m.keyCount() + (m.hasLcd() ? m.encoders : 0);
}

QSize faceSize(const Model& m, int face) noexcept
{
  if(face < m.keyCount())
  {
    if(m.smallColumn >= 0 && m.columns > 0 && face % m.columns == m.smallColumn)
      return {m.smallKeySize, m.smallKeySize};
    return {m.keyWidth, m.keyHeight};
  }
  return {m.lcdWidth / std::max(1, m.encoders), m.lcdHeight};
}

QSize canvasSize(const Model& m) noexcept
{
  if(!m.hasDisplay())
    return {};
  const int gap = gapFor(m);
  const int w = m.columns * m.keyWidth + (m.columns - 1) * gap;
  int h = m.rows * m.keyHeight + (m.rows - 1) * gap;
  if(m.hasLcd())
    h += gap + w * m.lcdHeight / m.lcdWidth;
  return {w, h};
}

QRect tileRect(const Model& m, int face) noexcept
{
  const int gap = gapFor(m);
  if(face < m.keyCount())
  {
    const int col = face % m.columns;
    const int row = face / m.columns;
    return {col * (m.keyWidth + gap), row * (m.keyHeight + gap), m.keyWidth, m.keyHeight};
  }

  const auto canvas = canvasSize(m);
  const int keysHeight = m.rows * m.keyHeight + (m.rows - 1) * gap;
  const int segment = face - m.keyCount();
  const int segmentWidth = canvas.width() / std::max(1, m.encoders);
  return {
      segment * segmentWidth, keysHeight + gap, segmentWidth,
      canvas.height() - keysHeight - gap};
}

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
QImage orient(const QImage& img, const Model& m)
{
  QImage res = img;
  if(m.rotation % 360 != 0)
    res = res.transformed(QTransform().rotate(m.rotation));
  if(m.flipX || m.flipY)
  {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    res = res.flipped(
        (m.flipX ? Qt::Horizontal : Qt::Orientations{})
        | (m.flipY ? Qt::Vertical : Qt::Orientations{}));
#else
    res = res.mirrored(m.flipX, m.flipY);
#endif
  }
  return res;
}

QByteArray encode(const QImage& img, ImageFormat format, int maxBytes)
{
  QByteArray out;
  QBuffer buf{&out};
  buf.open(QIODevice::WriteOnly);
  const auto rgb = img.convertToFormat(QImage::Format_RGB888);
  if(format == ImageFormat::BMP)
  {
    rgb.save(&buf, "BMP");
    return out;
  }
  for(int quality = 90;; quality -= 15)
  {
    out.clear();
    buf.seek(0);
    rgb.save(&buf, "JPEG", quality);
    if(maxBytes <= 0 || out.size() <= maxBytes || quality <= 30)
      return out;
  }
}
}
