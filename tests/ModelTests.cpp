#include <StreamDeck/Model.hpp>
#include <StreamDeck/Render.hpp>

#include <QGuiApplication>
#include <QImage>
#include <QPainter>

#include <catch2/catch_all.hpp>

#include <cstring>
#include <map>
#include <set>

using namespace StreamDeck;

namespace
{
QGuiApplication& app()
{
  static int argc = 1;
  static char arg0[] = "streamdeck_models_test";
  static char* argv[] = {arg0, nullptr};
  static QGuiApplication a{argc, argv};
  return a;
}

std::string label(const Model& m)
{
  return std::string{m.name};
}

//! Reassembles the payload of the image reports of a key, from their headers.
struct Upload
{
  int key{-1};
  std::size_t size{};
  std::vector<uint8_t> payload;
  bool complete{};
};

Upload reassemble(const Model& m, const std::vector<Report>& reports)
{
  Upload up;
  int expectedPage = 0;
  for(const auto& rep : reports)
  {
    const auto& r = rep.data;
    REQUIRE_FALSE(rep.feature);
    REQUIRE(r.size() == std::size_t(m.reportSize));
    REQUIRE_FALSE(up.complete);
    switch(m.protocol)
    {
      case Protocol::ElgatoGen1: {
        REQUIRE(r[0] == 0x02);
        REQUIRE(r[1] == 0x01);
        REQUIRE(r[2] == expectedPage);
        const int key = r[5] - 1;
        if(up.key < 0)
          up.key = key;
        REQUIRE(key == up.key);
        up.payload.insert(up.payload.end(), r.begin() + 16, r.end());
        up.complete = r[4] == 1;
        break;
      }
      case Protocol::ElgatoGen2: {
        REQUIRE(r[0] == 0x02);
        REQUIRE(r[1] == 0x07);
        const int len = r[4] | (r[5] << 8);
        const int page = r[6] | (r[7] << 8);
        REQUIRE(page == expectedPage);
        REQUIRE(len <= m.reportSize - 8);
        if(up.key < 0)
          up.key = r[2];
        REQUIRE(r[2] == up.key);
        up.payload.insert(up.payload.end(), r.begin() + 8, r.begin() + 8 + len);
        up.complete = r[3] == 1;
        break;
      }
      case Protocol::Mirabox: {
        REQUIRE(r[0] == 0x00);
        if(expectedPage == 0)
        {
          REQUIRE(std::string(r.begin() + 1, r.begin() + 9) == std::string("CRT\0\0BAT", 8));
          up.size = (r[9] << 24) | (r[10] << 16) | (r[11] << 8) | r[12];
          up.key = r[13];
        }
        else
        {
          up.payload.insert(up.payload.end(), r.begin() + 1, r.end());
          up.complete = up.payload.size() >= up.size;
        }
        break;
      }
    }
    expectedPage++;
  }
  if(m.protocol == Protocol::Mirabox)
  {
    REQUIRE(up.payload.size() < up.size + m.reportSize);
    up.payload.resize(up.size);
  }
  return up;
}

QImage testPattern(QSize size)
{
  QImage img{size, QImage::Format_RGB888};
  img.fill(Qt::black);
  QPainter p{&img};
  const int w = size.width() / 2, h = size.height() / 2;
  p.fillRect(0, 0, w, h, Qt::red);
  p.fillRect(w, 0, size.width() - w, h, Qt::green);
  p.fillRect(0, h, w, size.height() - h, Qt::blue);
  return img;
}

QImage unorient(QImage img, const Model& m)
{
  if(m.flipX || m.flipY)
    img = img.mirrored(m.flipX, m.flipY);
  if(m.rotation % 360 != 0)
    img = img.transformed(QTransform().rotate(-m.rotation));
  return img;
}

bool near(QColor a, QColor b)
{
  return std::abs(a.red() - b.red()) < 60 && std::abs(a.green() - b.green()) < 60
         && std::abs(a.blue() - b.blue()) < 60;
}
}

TEST_CASE("Every model is unique and consistent", "[streamdeck]")
{
  std::map<std::pair<int, int>, std::string> ids;
  for(const auto& m : models())
  {
    INFO(label(m));
    CHECK(ids.emplace(std::make_pair(m.vendorId, m.productId), label(m)).second);
    CHECK(findModel(m.vendorId, m.productId) == &m);
    CHECK(isKnownVendor(m.vendorId));
    CHECK(m.keyCount() > 0);
    CHECK(m.reportSize >= 64);
    CHECK((m.keyMap.empty() || int(m.keyMap.size()) == m.keyCount()));
    CHECK((m.keyWidth > 0) == (m.keyHeight > 0));
    CHECK((m.lcdWidth > 0) == (m.lcdHeight > 0));
    CHECK(m.rotation % 90 == 0);
  }
}

TEST_CASE("Faces tile the texture of every model", "[streamdeck]")
{
  for(const auto& m : models())
  {
    INFO(label(m));
    if(!m.hasDisplay())
    {
      CHECK(faceCount(m) == 0);
      CHECK(canvasSize(m).isEmpty());
      continue;
    }

    const auto canvas = canvasSize(m);
    REQUIRE(!canvas.isEmpty());
    const QRect bounds{QPoint{}, canvas};
    const int n = faceCount(m);
    CHECK(n == m.keyCount() + (m.hasLcd() ? m.encoders : 0));
    for(int i = 0; i < n; i++)
    {
      INFO("face " << i);
      const auto tile = tileRect(m, i);
      const auto size = faceSize(m, i);
      CHECK(!size.isEmpty());
      CHECK(bounds.contains(tile));
      if(i < m.keyCount() && i % m.columns != m.smallColumn)
        CHECK(tile.size() == size);
      else
        CHECK(std::abs(
                  double(tile.width()) / tile.height() - double(size.width()) / size.height())
              < 0.1);
      for(int j = i + 1; j < n; j++)
        CHECK_FALSE(tile.intersects(tileRect(m, j)));
    }
  }
}

TEST_CASE("Key images survive the trip to every model", "[streamdeck]")
{
  app();
  for(const auto& m : models())
  {
    if(!m.hasDisplay())
      continue;
    INFO(label(m));
    const QSize size = faceSize(m, 0);
    const auto pattern = testPattern(size);
    const auto oriented = orient(pattern, m);
    const auto bytes = encode(oriented, m.format);
    REQUIRE(bytes.size() > 0);

    for(int key : {0, m.keyCount() - 1})
    {
      INFO("key " << key);
      const auto reports = keyImageReports(
          m, key,
          {reinterpret_cast<const uint8_t*>(bytes.constData()), std::size_t(bytes.size())});
      REQUIRE(!reports.empty());
      const auto up = reassemble(m, reports);
      CHECK(up.complete);
      CHECK(up.key == m.deviceKey(key));
      REQUIRE(up.payload.size() >= std::size_t(bytes.size()));

      QImage decoded;
      REQUIRE(decoded.loadFromData(up.payload.data(), int(up.payload.size())));
      REQUIRE(decoded.size() == oriented.size());

      const auto upright = unorient(decoded, m);
      REQUIRE(upright.size() == size);
      const int qx = size.width() / 4, qy = size.height() / 4;
      CHECK(near(upright.pixelColor(qx, qy), Qt::red));
      CHECK(near(upright.pixelColor(3 * qx, qy), Qt::green));
      CHECK(near(upright.pixelColor(qx, 3 * qy), Qt::blue));
    }
  }
}

TEST_CASE("Images larger than one report are split in order", "[streamdeck]")
{
  for(const auto& m : models())
  {
    if(!m.hasDisplay() || m.protocol == Protocol::ElgatoGen1)
      continue;
    INFO(label(m));
    std::vector<uint8_t> big(5000);
    for(std::size_t i = 0; i < big.size(); i++)
      big[i] = uint8_t(i * 7 + 3);
    const auto up = reassemble(m, keyImageReports(m, 1, big));
    CHECK(up.complete);
    CHECK(up.payload == big);
  }
}

TEST_CASE("Every key press is read back as that key", "[streamdeck]")
{
  for(const auto& m : models())
  {
    INFO(label(m));
    for(int key = 0; key < m.keyCount(); key++)
    {
      INFO("key " << key);
      std::vector<uint8_t> r(512, 0);
      const int deviceKey = m.deviceKey(key);
      switch(m.protocol)
      {
        case Protocol::Mirabox:
          std::memcpy(r.data(), "ACK\0\0OK", 7);
          r[9] = m.inputMap.empty() ? m.keyMap[key] : m.inputMap[key];
          r[10] = 0x01;
          break;
        case Protocol::ElgatoGen1:
          r[0] = 0x01;
          r[1 + deviceKey] = 1;
          break;
        case Protocol::ElgatoGen2:
          r[0] = 0x01;
          r[1] = 0x00;
          r[4 + deviceKey] = 1;
          break;
      }
      const auto ev = parseInput(m, r);
      if(ev.kind == InputEvent::Keys)
      {
        REQUIRE(int(ev.values.size()) >= m.keyCount());
        for(int k = 0; k < m.keyCount(); k++)
          CHECK(ev.values[k] == (k == key));
      }
      else
      {
        REQUIRE(ev.kind == InputEvent::Key);
        REQUIRE(ev.values.size() == 2);
        CHECK(ev.values[0] == key);
        CHECK(ev.values[1] == 1);
        CHECK(ev.tap == m.pressOnly);
      }
    }
  }
}

TEST_CASE("Text fits keys of every size", "[streamdeck]")
{
  app();
  ImageCache cache;
  for(const auto& m : models())
  {
    if(!m.hasDisplay())
      continue;
    INFO(label(m));
    for(int i = 0; i < faceCount(m); i++)
    {
      Style s;
      s.background = Qt::black;
      s.textColor = Qt::white;
      s.text = QStringLiteral("BLACKOUT\nhold 1s");
      const auto size = faceSize(m, i);
      const auto img = renderStyle(s, size, cache);
      REQUIRE(img.size() == size);

      int minX = size.width(), maxX = -1;
      for(int y = 0; y < size.height(); y++)
        for(int x = 0; x < size.width(); x++)
          if(qGray(img.pixel(x, y)) > 128)
          {
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
          }
      CHECK(maxX >= 0);
      CHECK(minX > 0);
      CHECK(maxX < size.width() - 1);
    }
  }
}

TEST_CASE("Mirabox maps are permutations, inputs are unambiguous", "[streamdeck]")
{
  for(const auto& m : models())
  {
    if(m.protocol != Protocol::Mirabox)
      continue;
    INFO(label(m));
    REQUIRE(int(m.keyMap.size()) == m.keyCount());
    std::set<int> ids(m.keyMap.begin(), m.keyMap.end());
    CHECK(int(ids.size()) == m.keyCount());
    CHECK(*ids.begin() >= 1);
    for(auto s : m.stripMap)
      CHECK(ids.count(s) == 0);

    std::multiset<int> codes;
    for(auto c : m.inputMap.empty() ? m.keyMap : m.inputMap)
      codes.insert(c);
    for(auto c : m.touchKeyCodes)
      codes.insert(c);
    for(auto c : m.stripTapCodes)
      codes.insert(c);
    for(auto d : m.dialCodes)
    {
      codes.insert(d.left);
      codes.insert(d.right);
      codes.insert(d.press);
    }
    for(auto c : codes)
    {
      INFO("code " << c);
      CHECK(codes.count(c) == 1);
      CHECK(c != 0xFF);
    }
    CHECK(int(m.touchKeyCodes.size()) == m.touchKeys);
    CHECK(int(m.dialCodes.size()) == m.encoders);
    if(m.hasLcd())
      CHECK(int(m.stripMap.size()) == m.encoders);
  }
}

TEST_CASE("Mirabox dials, strip and side buttons are decoded", "[streamdeck]")
{
  for(const auto& m : models())
  {
    if(m.protocol != Protocol::Mirabox)
      continue;
    INFO(label(m));
    auto packet = [](uint8_t code, uint8_t state) {
      std::vector<uint8_t> r(512, 0);
      std::memcpy(r.data(), "ACK\0\0OK", 7);
      r[9] = code;
      r[10] = state;
      return r;
    };
    for(int i = 0; i < int(m.dialCodes.size()); i++)
    {
      auto l = parseInput(m, packet(m.dialCodes[i].left, 1));
      REQUIRE(l.kind == InputEvent::EncoderTurn);
      CHECK(l.values[i] == -1);
      auto r = parseInput(m, packet(m.dialCodes[i].right, 1));
      REQUIRE(r.kind == InputEvent::EncoderTurn);
      CHECK(r.values[i] == 1);
      auto p = parseInput(m, packet(m.dialCodes[i].press, 1));
      REQUIRE(p.kind == InputEvent::EncoderPress);
      CHECK(p.values[i] == 1);
      CHECK(p.tap);
      if(!m.pressOnly)
        CHECK(parseInput(m, packet(m.dialCodes[i].press, 2)).kind == InputEvent::Unknown);
    }
    for(int i = 0; i < int(m.touchKeyCodes.size()); i++)
    {
      auto ev = parseInput(m, packet(m.touchKeyCodes[i], 1));
      REQUIRE(ev.kind == InputEvent::Key);
      CHECK(ev.values[0] == m.keyCount() + i);
    }
    for(int i = 0; i < int(m.stripTapCodes.size()); i++)
    {
      auto ev = parseInput(m, packet(m.stripTapCodes[i], 1));
      REQUIRE(ev.kind == InputEvent::TouchShort);
      CHECK(ev.x * m.encoders / m.lcdWidth == i);
    }
    CHECK(parseInput(m, packet(0xFF, 1)).kind == InputEvent::Unknown);
    if(!m.pressOnly)
    {
      auto ev = parseInput(m, packet(m.inputMap.empty() ? m.keyMap[0] : m.inputMap[0], 0x02));
      REQUIRE(ev.kind == InputEvent::Key);
      CHECK(ev.values[1] == 0);
    }
  }
}

TEST_CASE("Mirabox commands are framed", "[streamdeck]")
{
  for(const auto& m : models())
  {
    if(m.protocol != Protocol::Mirabox)
      continue;
    INFO(label(m));
    auto check = [&](const std::vector<Report>& reps, std::string_view cmd) {
      REQUIRE(!reps.empty());
      const auto& r = reps.back().data;
      CHECK_FALSE(reps.back().feature);
      REQUIRE(int(r.size()) == m.reportSize);
      CHECK(std::string(r.begin(), r.begin() + 6) == std::string("\0CRT\0\0", 6));
      CHECK(std::string(r.begin() + 6, r.begin() + 6 + cmd.size()) == cmd);
    };
    check(brightnessReports(m, 42), std::string_view{"LIG\0\0\x2a", 6});
    check(flushReports(m), "STP");
    check(keepAliveReports(m), "CONNECT");
    check(openReports(m), "STP");
    CHECK(keepAliveInterval(m) > 0);
    CHECK(keepAliveInterval(m) <= 10000);
  }
}

TEST_CASE("The Mini is fed transposed images", "[streamdeck]")
{
  app();
  const auto* m = findModel(elgato_vendor_id, 0x0063);
  REQUIRE(m);
  const auto img = testPattern({80, 80});
  const auto o = orient(img, *m);
  for(int y = 0; y < 80; y += 7)
    for(int x = 0; x < 80; x += 7)
      CHECK(o.pixel(x, y) == img.pixel(y, x));
}

TEST_CASE("Elgato gen2 decks with keys reset their image stream on open", "[streamdeck]")
{
  for(const auto& m : models())
  {
    if(m.protocol != Protocol::ElgatoGen2)
      continue;
    INFO(label(m));
    const auto open = openReports(m);
    CHECK(open.size() == (m.hasDisplay() ? 1u : 0u));
    const auto bright = brightnessReports(m, 50);
    REQUIRE(bright.size() == 1);
    CHECK(bright[0].feature);
    CHECK(bright[0].data.size() == 32);
  }
}
