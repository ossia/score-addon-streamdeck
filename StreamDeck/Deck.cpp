#include "Deck.hpp"


namespace StreamDeck
{
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
    for(const auto& r : closeReports(m_model))
      send(r);
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

void Deck::send(const Report& r)
{
  if(r.feature)
    m_hid.send_feature_report(r.data);
  else
    m_hid.write(r.data);
}

std::string Deck::readString(StringReport r)
{
  if(r.length <= 0)
    return {};
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

    case InputEvent::Key:
      if(ev.values.size() == 2 && ev.values[0] >= 0
         && ev.values[0] < int(m_keyState.size()) && m_keyState[ev.values[0]] != ev.values[1])
      {
        const int k = ev.values[0];
        auto notify = [&](bool p) {
          m_keyState[k] = p;
          if(k < m_model.keyCount())
          {
            if(m_callbacks.key)
              m_callbacks.key(k, p);
          }
          else if(m_callbacks.touchKey)
            m_callbacks.touchKey(k - m_model.keyCount(), p);
        };
        notify(ev.values[1]);
        if(ev.tap && ev.values[1])
          notify(false);
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
        if(ev.tap && ev.values[i])
        {
          m_encoderState[i] = 0;
          if(m_callbacks.encoderPress)
            m_callbacks.encoderPress(i, false);
        }
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
  const auto bytes = encode(
      orient(img, m_model), m_model.format,
      m_model.protocol == Protocol::Mirabox ? 10240 : 0);
  const std::span<const uint8_t> data{
      reinterpret_cast<const uint8_t*>(bytes.constData()), std::size_t(bytes.size())};
  const auto reports = index < m_model.keyCount()
                           ? keyImageReports(m_model, index, data)
                           : stripImageReports(m_model, index - m_model.keyCount(), data);
  for(const auto& r : reports)
    send(r);
}

void Deck::workerLoop()
{
  for(const auto& r : openReports(m_model))
    send(r);

  const auto keepAlive = std::chrono::milliseconds{keepAliveInterval(m_model)};
  auto nextKeepAlive = std::chrono::steady_clock::now() + keepAlive;
  for(;;)
  {
    std::vector<std::pair<int, Style>> faces;
    std::vector<bool> textured;
    QImage frame;
    std::optional<int> brightness;
    {
      std::unique_lock lock{m_mutex};
      auto ready = [this] {
        return m_stop || m_pendingBrightness || !m_pendingFrame.isNull()
               || std::any_of(m_faces.begin(), m_faces.end(), [](const auto& f) {
          return f.dirty && !f.texture;
        });
      };
      if(keepAlive.count() > 0)
      {
        const bool woken = m_cv.wait_until(lock, nextKeepAlive, ready);
        if(std::chrono::steady_clock::now() >= nextKeepAlive)
        {
          lock.unlock();
          for(const auto& r : keepAliveReports(m_model))
            send(r);
          nextKeepAlive = std::chrono::steady_clock::now() + keepAlive;
          if(!woken)
            continue;
          lock.lock();
        }
        else if(!woken)
          continue;
      }
      else
      {
        m_cv.wait(lock, ready);
      }
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
      for(const auto& r : brightnessReports(m_model, *brightness))
        send(r);

    bool uploaded = !faces.empty();
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
        uploaded = true;
      }
    }

    if(uploaded)
      for(const auto& r : flushReports(m_model))
        send(r);
  }
}
}
