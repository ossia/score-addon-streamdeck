#include "Deck.hpp"

#include <QBuffer>
#include <QTransform>

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

int faceCount(const Model& m) noexcept
{
  if(!m.hasDisplay())
    return 0;
  return m.keyCount() + (m.hasLcd() ? m.encoders : 0);
}

QSize faceSize(const Model& m, int face) noexcept
{
  if(face < m.keyCount())
    return {m.keyWidth, m.keyHeight};
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

Deck::Deck(const Model& model, const ossia::net::hid_device_configuration& conf)
    : m_model{model}
{
  m_hid.open(conf);
  m_keyState.resize(m_model.keyCount() + m_model.touchKeys);
  m_encoderState.resize(m_model.encoders);
  m_faces.resize(faceCount(m_model));
  m_lastTiles.resize(faceCount(m_model));
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

  if(m_hid.is_open() && !m_unplugged)
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

void Deck::setFace(int index, const Style& f)
{
  std::lock_guard lock{m_mutex};
  if(index < 0 || index >= int(m_faces.size()))
    return;
  auto& face = m_faces[index];
  if(face.face == f && !face.dirty)
    return;
  face.face = f;
  face.dirty = true;
  m_cv.notify_one();
}

void Deck::setTextureMask(std::vector<bool> mask)
{
  std::lock_guard lock{m_mutex};
  for(std::size_t i = 0; i < m_faces.size(); i++)
  {
    const bool t = i < mask.size() && mask[i];
    if(t == m_faces[i].texture)
      continue;
    m_faces[i].texture = t;
    m_faces[i].dirty = !t;
  }
  m_cv.notify_one();
}

void Deck::setBrightness(int percent)
{
  std::lock_guard lock{m_mutex};
  m_pendingBrightness = percent;
  m_cv.notify_one();
}

void Deck::submitFrame(const QImage& frame)
{
  if(frame.size() != canvasSize(m_model))
    return;
  std::lock_guard lock{m_mutex};
  m_pendingFrame = frame;
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

void Deck::upload(int index, const QImage& img)
{
  if(index < m_model.keyCount())
  {
    const auto bytes = encode(orient(img, m_model), m_model.format);
    for(const auto& r : keyImageReports(
            m_model, index,
            {reinterpret_cast<const uint8_t*>(bytes.constData()), std::size_t(bytes.size())}))
      m_hid.write(r);
  }
  else
  {
    const auto bytes = encode(img, ImageFormat::JPEG);
    const int segment = index - m_model.keyCount();
    const auto sz = faceSize(m_model, index);
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
    std::vector<std::pair<int, Style>> faces;
    std::vector<bool> textured;
    QImage frame;
    std::optional<int> brightness;
    {
      std::unique_lock lock{m_mutex};
      m_cv.wait(lock, [this] {
        return m_stop || m_pendingBrightness || !m_pendingFrame.isNull()
               || std::any_of(m_faces.begin(), m_faces.end(), [](const auto& f) {
          return f.dirty && !f.texture;
        });
      });
      if(m_stop)
        return;

      brightness = std::exchange(m_pendingBrightness, std::nullopt);
      std::swap(frame, m_pendingFrame);
      textured.resize(m_faces.size());
      for(int i = 0; i < int(m_faces.size()); i++)
      {
        auto& f = m_faces[i];
        textured[i] = f.texture;
        if(f.dirty && !f.texture)
        {
          faces.emplace_back(i, f.face);
          f.dirty = false;
          m_lastTiles[i] = QImage{};
        }
      }
    }

    if(brightness)
      m_hid.send_feature_report(brightnessReport(m_model, *brightness));

    for(auto& [index, face] : faces)
      upload(index, renderStyle(face, faceSize(m_model, index), m_images));

    if(!frame.isNull())
    {
      for(int i = 0; i < int(textured.size()); i++)
      {
        if(!textured[i])
          continue;
        auto tile = frame.copy(tileRect(m_model, i));
        const auto sz = faceSize(m_model, i);
        if(tile.size() != sz)
          tile = tile.scaled(sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if(tile == m_lastTiles[i])
          continue;
        upload(i, tile);
        m_lastTiles[i] = std::move(tile);
      }
    }
  }
}
}
