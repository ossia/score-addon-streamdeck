#include "Deck.hpp"

#include <QBuffer>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QTransform>

#include <stdexcept>

namespace StreamDeck
{
namespace
{
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

QByteArray encode(const QImage& img, ImageFormat format)
{
  QByteArray out;
  QBuffer buf{&out};
  buf.open(QIODevice::WriteOnly);
  const auto rgb = img.convertToFormat(QImage::Format_RGB888);
  if(format == ImageFormat::BMP)
    rgb.save(&buf, "BMP");
  else
    rgb.save(&buf, "JPEG", 90);
  return out;
}

int gapFor(const Model& m)
{
  return m.keyWidth / 4;
}
}

Deck::Deck(const Model& model, const ossia::net::hid_device_configuration& conf)
    : m_model{model}
{
  m_hid.open(conf);
  m_keyState.resize(m_model.keyCount() + m_model.touchKeys);
  m_encoderState.resize(m_model.encoders);
  m_faces.resize(faceCount());
  m_cache.resize(faceCount());
}

Deck::~Deck()
{
  stop();
}

void Deck::stop()
{
  {
    std::lock_guard lock{m_mutex};
    m_stop = true;
  }
  m_cv.notify_all();
  if(m_worker.joinable())
    m_worker.join();

  if(m_hid.is_open())
    m_hid.send_feature_report(resetReport(m_model));
  m_hid.close();
}

void Deck::start(Callbacks cb, int brightness)
{
  m_callbacks = std::move(cb);
  m_pendingBrightness = brightness;
  m_worker = std::thread{[this] { workerLoop(); }};
  m_hid.start(
      [this](std::span<const uint8_t> r) { onInput(r); },
      [this](std::string_view) {
    m_unplugged = true;
    if(m_callbacks.unplugged)
      m_callbacks.unplugged();
  },
      inputReportSize(m_model));
}

std::string Deck::readString(StringReport r)
{
  std::vector<uint8_t> buf(r.length, 0);
  buf[0] = r.id;
  const int n = m_hid.get_feature_report(buf);
  if(n <= r.offset)
    return {};
  std::string s;
  for(int i = r.offset; i < n && buf[i] != 0; i++)
    if(buf[i] >= 0x20 && buf[i] < 0x7f)
      s += char(buf[i]);
  return s;
}

std::string Deck::serial()
{
  if(!m_hid.info().serial.empty())
    return m_hid.info().serial;
  return readString(serialReport(m_model));
}

std::string Deck::firmware()
{
  return readString(firmwareReport(m_model));
}

int Deck::faceCount() const noexcept
{
  if(!m_model.hasDisplay())
    return 0;
  return m_model.keyCount() + (m_model.hasLcd() ? m_model.encoders : 0);
}

void Deck::setFace(int index, const Face& f)
{
  updateFace(index, [&](Face& face) { face = f; });
}

void Deck::clear()
{
  std::lock_guard lock{m_mutex};
  for(auto& f : m_faces)
  {
    f.face = Face{};
    f.dirty = true;
  }
  m_cv.notify_one();
}

void Deck::setBrightness(int percent)
{
  std::lock_guard lock{m_mutex};
  m_pendingBrightness = percent;
  m_cv.notify_one();
}

QSize Deck::faceSize(int index) const noexcept
{
  if(index < m_model.keyCount())
    return {m_model.keyWidth, m_model.keyHeight};
  return {m_model.lcdWidth / std::max(1, m_model.encoders), m_model.lcdHeight};
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

QSize Deck::canvasSize() const noexcept
{
  return StreamDeck::canvasSize(m_model);
}

QRect Deck::tileRect(int index) const noexcept
{
  const int gap = gapFor(m_model);
  if(index < m_model.keyCount())
  {
    const int col = index % m_model.columns;
    const int row = index / m_model.columns;
    return {
        col * (m_model.keyWidth + gap), row * (m_model.keyHeight + gap),
        m_model.keyWidth, m_model.keyHeight};
  }

  const auto canvas = canvasSize();
  const int keysHeight = m_model.rows * m_model.keyHeight + (m_model.rows - 1) * gap;
  const int segment = index - m_model.keyCount();
  const int segmentWidth = canvas.width() / std::max(1, m_model.encoders);
  return {
      segment * segmentWidth, keysHeight + gap, segmentWidth,
      canvas.height() - keysHeight - gap};
}

void Deck::submitFrame(const uint8_t* rgba, int width, int height, int bytesPerLine)
{
  if(width != canvasSize().width() || height != canvasSize().height())
    return;

  QImage frame{rgba, width, height, bytesPerLine, QImage::Format_RGBA8888};
  std::lock_guard lock{m_mutex};
  m_pendingFrame = frame.copy();
  m_cv.notify_one();
}

void Deck::onInput(std::span<const uint8_t> report)
{
  const auto ev = parseInput(m_model, report);
  switch(ev.kind)
  {
    case InputEvent::Keys:
      for(std::size_t i = 0; i < ev.values.size() && i < m_keyState.size(); i++)
      {
        if(ev.values[i] == m_keyState[i])
          continue;
        m_keyState[i] = ev.values[i];
        const int keys = m_model.keyCount();
        if(int(i) < keys)
        {
          if(m_callbacks.key)
            m_callbacks.key(i, ev.values[i]);
        }
        else if(m_callbacks.touchKey)
          m_callbacks.touchKey(i - keys, ev.values[i]);
      }
      break;

    case InputEvent::EncoderPress:
      for(std::size_t i = 0; i < ev.values.size() && i < m_encoderState.size(); i++)
      {
        if(ev.values[i] == m_encoderState[i])
          continue;
        m_encoderState[i] = ev.values[i];
        if(m_callbacks.encoderPress)
          m_callbacks.encoderPress(i, ev.values[i]);
      }
      break;

    case InputEvent::EncoderTurn:
      for(std::size_t i = 0; i < ev.values.size(); i++)
        if(ev.values[i] != 0 && m_callbacks.encoderTurn)
          m_callbacks.encoderTurn(i, ev.values[i]);
      break;

    case InputEvent::TouchShort:
    case InputEvent::TouchLong:
    case InputEvent::TouchSwipe:
      if(m_callbacks.touch)
        m_callbacks.touch(ev.kind, ev.x, ev.y, ev.x2, ev.y2);
      break;

    case InputEvent::Unknown:
      break;
  }
}

QImage Deck::renderFace(const Face& f, WorkerCache& cache, QSize size)
{
  QImage img{size, QImage::Format_ARGB32_Premultiplied};
  img.fill(f.background);

  QPainter p{&img};
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);

  if(f.imagePath != cache.path)
  {
    cache.path = f.imagePath;
    cache.image = f.imagePath.isEmpty() ? QImage{} : QImage{f.imagePath};
  }
  if(!cache.image.isNull())
  {
    auto scaled = cache.image.scaled(
        size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    p.drawImage(
        (size.width() - scaled.width()) / 2, (size.height() - scaled.height()) / 2,
        scaled);
  }

  if(!f.text.isEmpty())
  {
    const int margin = std::max(2, size.width() / 16);
    const QRect area = img.rect().adjusted(margin, margin, -margin, -margin);
    constexpr int flags = Qt::AlignCenter | Qt::TextWordWrap;

    QFont font;
    font.setBold(true);
    int px = f.textSize > 0 ? f.textSize : std::max(8, size.height() * 3 / 10);
    font.setPixelSize(px);
    if(f.textSize <= 0)
    {
      while(px > 8)
      {
        const auto r = QFontMetrics{font}.boundingRect(area, flags, f.text);
        if(r.width() <= area.width() && r.height() <= area.height())
          break;
        font.setPixelSize(--px);
      }
    }
    p.setFont(font);
    p.setPen(f.textColor);
    p.drawText(area, flags, f.text);
  }

  return img;
}

void Deck::upload(int index, const QImage& img)
{
  const auto oriented = orient(img, m_model);
  if(index < m_model.keyCount())
  {
    const auto bytes = encode(oriented, m_model.format);
    for(const auto& r : keyImageReports(
            m_model, index,
            {reinterpret_cast<const uint8_t*>(bytes.constData()), std::size_t(bytes.size())}))
      m_hid.write(r);
  }
  else
  {
    const auto bytes = encode(img, ImageFormat::JPEG);
    const int segment = index - m_model.keyCount();
    const auto sz = faceSize(index);
    for(const auto& r : lcdImageReports(
            m_model, segment * sz.width(), 0, sz.width(), sz.height(),
            {reinterpret_cast<const uint8_t*>(bytes.constData()), std::size_t(bytes.size())}))
      m_hid.write(r);
  }
}

void Deck::workerLoop()
{
  for(;;)
  {
    std::vector<std::pair<int, Face>> faces;
    QImage frame;
    std::optional<int> brightness;
    {
      std::unique_lock lock{m_mutex};
      m_cv.wait(lock, [this] {
        return m_stop || m_pendingBrightness || !m_pendingFrame.isNull()
               || std::any_of(m_faces.begin(), m_faces.end(), [](const auto& f) {
          return f.dirty;
        });
      });
      if(m_stop)
        return;

      brightness = std::exchange(m_pendingBrightness, std::nullopt);
      std::swap(frame, m_pendingFrame);
      for(int i = 0; i < int(m_faces.size()); i++)
      {
        if(m_faces[i].dirty)
        {
          faces.emplace_back(i, m_faces[i].face);
          m_faces[i].dirty = false;
        }
      }
    }

    if(brightness)
      m_hid.send_feature_report(brightnessReport(m_model, *brightness));

    for(auto& [index, face] : faces)
      upload(index, renderFace(face, m_cache[index], faceSize(index)));

    if(!frame.isNull())
    {
      for(int i = 0; i < int(m_cache.size()); i++)
      {
        auto tile = frame.copy(tileRect(i));
        if(tile.size() != faceSize(i))
          tile = tile.scaled(faceSize(i), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if(tile == m_cache[i].lastTile)
          continue;
        upload(i, tile);
        m_cache[i].lastTile = std::move(tile);
      }
    }
  }
}
}
