#include "Model.hpp"

#include <algorithm>
#include <cstring>
#include <initializer_list>

namespace StreamDeck
{
namespace
{
constexpr uint16_t mirabox_usage_page = 0xFFA0;

// Image number of each key, row by row, as the device counts them.
constexpr uint8_t akp153_keys[]
    = {13, 10, 7, 4, 1, 16, 14, 11, 8, 5, 2, 17, 15, 12, 9, 6, 3, 18};
constexpr uint8_t bottom_up_5x3[] = {11, 12, 13, 14, 15, 6, 7, 8, 9, 10, 1, 2, 3, 4, 5};
constexpr uint8_t top_down_5x3[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
constexpr uint8_t top_down_3x2[] = {1, 2, 3, 4, 5, 6};
constexpr uint8_t n4_keys[] = {11, 12, 13, 14, 15, 6, 7, 8, 9, 10};
constexpr uint8_t n4_inputs[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
constexpr uint8_t n4_strip[] = {1, 2, 3, 4};
constexpr uint8_t n4_strip_taps[] = {0x40, 0x41, 0x42, 0x43};
constexpr DialCodes n4_dials[]
    = {{0xA0, 0xA1, 0x37}, {0x50, 0x51, 0x35}, {0x90, 0x91, 0x33}, {0x70, 0x71, 0x36}};
constexpr uint8_t n3_buttons[] = {0x25, 0x30, 0x31};
constexpr DialCodes n3_dials[] = {{0x90, 0x91, 0x33}, {0x50, 0x51, 0x35}, {0x60, 0x61, 0x34}};

constexpr Model mk2(uint16_t pid, std::string_view name, bool verified = false)
{
  return {
      .productId = pid, .name = name, .columns = 5, .rows = 3, .keyWidth = 72,
      .keyHeight = 72, .flipX = true, .flipY = true, .verified = verified};
}
constexpr Model xl(uint16_t pid, std::string_view name)
{
  return {
      .productId = pid, .name = name, .columns = 8, .rows = 4, .keyWidth = 96,
      .keyHeight = 96, .flipX = true, .flipY = true};
}
constexpr Model mini(uint16_t pid, std::string_view name)
{
  return {
      .productId = pid, .name = name, .protocol = Protocol::ElgatoGen1, .columns = 3,
      .rows = 2, .keyWidth = 80, .keyHeight = 80, .format = ImageFormat::BMP,
      .rotation = 270, .flipY = true};
}

// AKP153 and its clones: 3 rows of 5 keys, and a column of 3 smaller ones.
constexpr Model akp153(uint16_t vid, uint16_t pid, std::string_view name)
{
  return {
      .vendorId = vid, .productId = pid, .name = name, .protocol = Protocol::Mirabox,
      .columns = 6, .rows = 3, .keyWidth = 85, .keyHeight = 85, .rotation = 270,
      .smallColumn = 5, .smallKeySize = 80, .reportSize = 513,
      .usagePage = mirabox_usage_page, .pressOnly = true, .keyMap = akp153_keys};
}
// 293 and its clones: 3 rows of 5, image numbers from the bottom row.
constexpr Model dock293(
    uint16_t vid, uint16_t pid, std::string_view name, int size, int reportSize,
    bool pressOnly, std::span<const uint8_t> inputs)
{
  return {
      .vendorId = vid, .productId = pid, .name = name, .protocol = Protocol::Mirabox,
      .columns = 5, .rows = 3, .keyWidth = size, .keyHeight = size, .rotation = 180,
      .reportSize = reportSize, .usagePage = mirabox_usage_page, .pressOnly = pressOnly,
      .keyMap = bottom_up_5x3, .inputMap = inputs};
}
// N3 / AKP03: 6 keys, 3 buttons, 3 dials.
constexpr Model n3(
    uint16_t vid, uint16_t pid, std::string_view name, int size, int rotation,
    bool pressOnly)
{
  return {
      .vendorId = vid, .productId = pid, .name = name, .protocol = Protocol::Mirabox,
      .columns = 3, .rows = 2, .keyWidth = size, .keyHeight = size,
      .rotation = rotation, .encoders = 3, .touchKeys = 3, .reportSize = 1025,
      .usagePage = mirabox_usage_page, .pressOnly = pressOnly, .keyMap = top_down_3x2,
      .touchKeyCodes = n3_buttons, .dialCodes = n3_dials};
}
// N4 / AKP05: 10 keys, a strip of 4 segments above 4 dials.
constexpr Model n4(uint16_t vid, uint16_t pid, std::string_view name)
{
  return {
      .vendorId = vid, .productId = pid, .name = name, .protocol = Protocol::Mirabox,
      .columns = 5, .rows = 2, .keyWidth = 112, .keyHeight = 112, .rotation = 180,
      .encoders = 4, .lcdWidth = 4 * 176, .lcdHeight = 112, .reportSize = 1025,
      .usagePage = mirabox_usage_page, .keyMap = n4_keys, .inputMap = n4_inputs,
      .stripMap = n4_strip, .stripTapCodes = n4_strip_taps, .dialCodes = n4_dials};
}

// clang-format off
constexpr Model model_table[] = {
  mk2(0x0080, "Stream Deck MK.2", true),
  mk2(0x00a5, "Stream Deck MK.2 (scissor keys)"),
  mk2(0x006d, "Stream Deck (v2)"),
  mk2(0x00b9, "Stream Deck MK.2 module"),
  mk2(0x00e4, "Stream Deck MK.2 module (scissor keys)"),
  xl(0x006c, "Stream Deck XL"),
  xl(0x008f, "Stream Deck XL (v2)"),
  xl(0x00ba, "Stream Deck XL module"),
  {.productId = 0x0084, .name = "Stream Deck +", .columns = 4, .rows = 2,
   .keyWidth = 120, .keyHeight = 120, .encoders = 4, .lcdWidth = 800, .lcdHeight = 100},
  {.productId = 0x009a, .name = "Stream Deck Neo", .columns = 4, .rows = 2,
   .keyWidth = 96, .keyHeight = 96, .flipX = true, .flipY = true, .touchKeys = 2},
  {.productId = 0x0086, .name = "Stream Deck Pedal", .columns = 3, .rows = 1},
  mini(0x0063, "Stream Deck Mini"),
  mini(0x0090, "Stream Deck Mini (v2)"),
  mini(0x00b3, "Stream Deck Mini (Discord edition)"),
  mini(0x00b8, "Stream Deck Mini module"),

  akp153(0x5548, 0x6674, "Ajazz AKP153"),
  akp153(0x0300, 0x1010, "Ajazz AKP153E"),
  akp153(0x0300, 0x1020, "Ajazz AKP153R"),
  akp153(0x5548, 0x6670, "Mirabox Stream Dock 293S"),

  dock293(0x5500, 0x1001, "Mirabox Stream Dock 293", 100, 513, true, bottom_up_5x3),
  dock293(0x0200, 0x1000, "Redragon SS-550", 100, 513, true, bottom_up_5x3),
  dock293(0x6603, 0x1005, "Mirabox Stream Dock 293 V3", 112, 1025, false, top_down_5x3),
  dock293(0x6603, 0x1006, "Mirabox Stream Dock 293 V3", 112, 1025, false, top_down_5x3),

  n3(0x6603, 0x1002, "Mirabox Stream Dock N3", 64, 90, false),
  n3(0x6603, 0x1003, "Mirabox Stream Dock N3", 64, 90, false),
  n3(0x1500, 0x3001, "Soomfon Stream Controller SE", 64, 90, false),
  n3(0x0300, 0x3003, "Ajazz AKP03R (rev. 2)", 64, 90, false),
  n3(0x0200, 0x2000, "Redragon SS-551", 64, 90, false),
  n3(0x0300, 0x1001, "Ajazz AKP03", 60, 0, true),
  n3(0x0300, 0x1003, "Ajazz AKP03R", 60, 0, true),

  n4(0x6602, 0x1001, "Mirabox Stream Dock N4"),
  n4(0x6603, 0x1007, "Mirabox Stream Dock N4"),
  n4(0x0300, 0x3004, "Ajazz AKP05E"),
  n4(0x0300, 0x3006, "Ajazz AKP05"),
  n4(0x1500, 0x3002, "Soomfon CN003"),
  n4(0x0200, 0x3001, "Redragon SS-552"),
};
// clang-format on

uint8_t lo(int v)
{
  return uint8_t(v & 0xFF);
}
uint8_t hi(int v)
{
  return uint8_t((v >> 8) & 0xFF);
}

int16_t le16(std::span<const uint8_t> r, std::size_t i)
{
  if(i + 1 >= r.size())
    return 0;
  return int16_t(r[i] | (r[i + 1] << 8));
}

//! A Mirabox command: report id 0, "CRT\0\0", then the command.
Report crt(const Model& m, std::initializer_list<uint8_t> cmd)
{
  std::vector<uint8_t> r(std::size_t(m.reportSize), 0);
  const uint8_t prefix[] = {0x00, 'C', 'R', 'T', 0x00, 0x00};
  std::copy(std::begin(prefix), std::end(prefix), r.begin());
  std::copy(cmd.begin(), cmd.end(), r.begin() + 6);
  return {.data = std::move(r)};
}

std::vector<Report> miraboxImage(const Model& m, int id, std::span<const uint8_t> image)
{
  std::vector<Report> reports;
  const auto size = uint32_t(image.size());
  reports.push_back(crt(
      m, {'B', 'A', 'T', uint8_t(size >> 24), uint8_t(size >> 16), uint8_t(size >> 8),
          uint8_t(size), uint8_t(id)}));

  const std::size_t payload = std::size_t(m.reportSize) - 1;
  for(std::size_t sent = 0; sent < image.size(); sent += payload)
  {
    std::vector<uint8_t> r(std::size_t(m.reportSize), 0);
    const std::size_t len = std::min(payload, image.size() - sent);
    std::copy_n(image.begin() + sent, len, r.begin() + 1);
    reports.push_back({.data = std::move(r)});
  }
  return reports;
}

int indexOf(std::span<const uint8_t> codes, uint8_t code)
{
  auto it = std::find(codes.begin(), codes.end(), code);
  return it == codes.end() ? -1 : int(it - codes.begin());
}

InputEvent parseMirabox(const Model& m, std::span<const uint8_t> r)
{
  InputEvent ev;
  if(r.size() < 11 || r[0] != 'A' || r[1] != 'C' || r[2] != 'K')
    return ev;
  const uint8_t code = r[9];
  if(code == 0xFF || code == 0x00)
    return ev;
  const bool pressed = m.pressOnly || r[10] == 0x01;
  const bool isDialPress = std::any_of(
      m.dialCodes.begin(), m.dialCodes.end(),
      [code](const DialCodes& d) { return d.press == code; });
  if(isDialPress && !pressed)
    return ev;

  const auto inputs = m.inputMap.empty() ? m.keyMap : m.inputMap;
  if(int k = indexOf(inputs, code); k >= 0)
  {
    ev.kind = InputEvent::Key;
    ev.values = {k, pressed};
    ev.tap = m.pressOnly;
    return ev;
  }
  if(int k = indexOf(m.touchKeyCodes, code); k >= 0)
  {
    ev.kind = InputEvent::Key;
    ev.values = {m.keyCount() + k, pressed};
    ev.tap = m.pressOnly;
    return ev;
  }
  for(int i = 0; i < int(m.dialCodes.size()); i++)
  {
    const auto& d = m.dialCodes[i];
    if(code == d.left || code == d.right)
    {
      ev.kind = InputEvent::EncoderTurn;
      ev.values.assign(m.encoders, 0);
      ev.values[i] = code == d.left ? -1 : 1;
      return ev;
    }
    if(code == d.press)
    {
      ev.kind = InputEvent::EncoderPress;
      ev.values.assign(m.encoders, 0);
      ev.values[i] = 1;
      ev.tap = true;
      return ev;
    }
  }
  if(int s = indexOf(m.stripTapCodes, code); s >= 0 && m.encoders > 0)
  {
    const int seg = m.lcdWidth / m.encoders;
    ev.kind = InputEvent::TouchShort;
    ev.x = s * seg + seg / 2;
    ev.y = m.lcdHeight / 2;
    return ev;
  }
  if((code == 0x38 || code == 0x39) && m.hasLcd())
  {
    ev.kind = InputEvent::TouchSwipe;
    ev.x = m.lcdWidth / 2;
    ev.y = ev.y2 = m.lcdHeight / 2;
    ev.x2 = code == 0x38 ? 0 : m.lcdWidth;
    return ev;
  }
  return ev;
}
}

std::span<const Model> models() noexcept
{
  return model_table;
}

const Model* findModel(uint16_t vendorId, uint16_t productId) noexcept
{
  for(const auto& m : model_table)
    if(m.vendorId == vendorId && m.productId == productId)
      return &m;
  return nullptr;
}

bool isKnownVendor(uint16_t vendorId) noexcept
{
  for(const auto& m : model_table)
    if(m.vendorId == vendorId)
      return true;
  return false;
}

int inputReportSize(const Model& m) noexcept
{
  return 1024;
}

InputEvent parseInput(const Model& m, std::span<const uint8_t> report)
{
  if(m.protocol == Protocol::Mirabox)
    return parseMirabox(m, report);

  InputEvent ev;
  if(report.empty())
    return ev;

  auto keys = [&](std::size_t offset, int count) {
    ev.kind = InputEvent::Keys;
    ev.values.resize(count);
    for(int i = 0; i < count; i++)
      ev.values[i] = offset + i < report.size() ? (report[offset + i] != 0) : 0;
  };

  if(m.protocol == Protocol::ElgatoGen1)
  {
    keys(1, m.keyCount());
    return ev;
  }

  if(report.size() < 2)
    return ev;

  switch(report[1])
  {
    case 0x00:
      keys(4, m.keyCount() + m.touchKeys);
      break;

    case 0x02: {
      if(report.size() < 10)
        break;
      switch(report[4])
      {
        case 1:
          ev.kind = InputEvent::TouchShort;
          break;
        case 2:
          ev.kind = InputEvent::TouchLong;
          break;
        case 3:
          ev.kind = InputEvent::TouchSwipe;
          ev.x2 = le16(report, 10);
          ev.y2 = le16(report, 12);
          break;
        default:
          return ev;
      }
      ev.x = le16(report, 6);
      ev.y = le16(report, 8);
      break;
    }

    case 0x03: {
      if(report.size() < std::size_t(5 + m.encoders) || report[4] > 1)
        break;
      ev.kind = report[4] == 0 ? InputEvent::EncoderPress : InputEvent::EncoderTurn;
      ev.values.resize(m.encoders);
      for(int i = 0; i < m.encoders; i++)
        ev.values[i] = ev.kind == InputEvent::EncoderPress ? int(report[5 + i] != 0)
                                                           : int(int8_t(report[5 + i]));
      break;
    }
  }
  return ev;
}

std::vector<Report>
keyImageReports(const Model& m, int key, std::span<const uint8_t> image)
{
  if(m.protocol == Protocol::Mirabox)
    return miraboxImage(m, m.deviceKey(key), image);

  std::vector<Report> reports;
  const std::size_t header = m.protocol == Protocol::ElgatoGen1 ? 16 : 8;
  const std::size_t payload = std::size_t(m.reportSize) - header;
  const int id = m.deviceKey(key);

  std::size_t sent = 0;
  int page = 0;
  do
  {
    const std::size_t len = std::min(payload, image.size() - sent);
    const bool last = sent + len == image.size();

    std::vector<uint8_t> r(std::size_t(m.reportSize), 0);
    r[0] = 0x02;
    if(m.protocol == Protocol::ElgatoGen1)
    {
      r[1] = 0x01;
      r[2] = uint8_t(page);
      r[4] = last;
      r[5] = uint8_t(id + 1);
    }
    else
    {
      r[1] = 0x07;
      r[2] = uint8_t(id);
      r[3] = last;
      r[4] = lo(len);
      r[5] = hi(len);
      r[6] = lo(page);
      r[7] = hi(page);
    }
    std::copy_n(image.begin() + sent, len, r.begin() + header);
    reports.push_back({.data = std::move(r)});

    sent += len;
    page++;
  } while(sent < image.size());

  return reports;
}

std::vector<Report>
stripImageReports(const Model& m, int segment, std::span<const uint8_t> image)
{
  std::vector<Report> reports;
  if(!m.hasLcd() || m.encoders <= 0 || segment < 0 || segment >= m.encoders)
    return reports;

  if(m.protocol == Protocol::Mirabox)
  {
    if(segment < int(m.stripMap.size()))
      reports = miraboxImage(m, m.stripMap[segment], image);
    return reports;
  }

  constexpr std::size_t header = 16;
  const std::size_t payload = std::size_t(m.reportSize) - header;
  const int w = m.lcdWidth / m.encoders;
  const int x = segment * w;
  const int y = 0;
  const int h = m.lcdHeight;

  std::size_t sent = 0;
  int page = 0;
  do
  {
    const std::size_t len = std::min(payload, image.size() - sent);
    const bool last = sent + len == image.size();

    std::vector<uint8_t> r(std::size_t(m.reportSize), 0);
    r[0] = 0x02;
    r[1] = 0x0C;
    r[2] = lo(x);
    r[3] = hi(x);
    r[4] = lo(y);
    r[5] = hi(y);
    r[6] = lo(w);
    r[7] = hi(w);
    r[8] = lo(h);
    r[9] = hi(h);
    r[10] = last;
    r[11] = lo(page);
    r[12] = hi(page);
    r[13] = lo(len);
    r[14] = hi(len);
    std::copy_n(image.begin() + sent, len, r.begin() + header);
    reports.push_back({.data = std::move(r)});

    sent += len;
    page++;
  } while(sent < image.size());

  return reports;
}

std::vector<Report> brightnessReports(const Model& m, int percent)
{
  const auto p = uint8_t(std::clamp(percent, 0, 100));
  switch(m.protocol)
  {
    case Protocol::ElgatoGen1: {
      std::vector<uint8_t> r(17, 0);
      r[0] = 0x05;
      r[1] = 0x55;
      r[2] = 0xaa;
      r[3] = 0xd1;
      r[4] = 0x01;
      r[5] = p;
      return {{.feature = true, .data = std::move(r)}};
    }
    case Protocol::ElgatoGen2: {
      std::vector<uint8_t> r(32, 0);
      r[0] = 0x03;
      r[1] = 0x08;
      r[2] = p;
      return {{.feature = true, .data = std::move(r)}};
    }
    case Protocol::Mirabox:
      return {crt(m, {'L', 'I', 'G', 0x00, 0x00, p})};
  }
  return {};
}

std::vector<Report> openReports(const Model& m)
{
  switch(m.protocol)
  {
    case Protocol::ElgatoGen2:
      if(m.hasDisplay())
      {
        std::vector<uint8_t> r(std::size_t(m.reportSize), 0);
        r[0] = 0x02;
        return {{.data = std::move(r)}};
      }
      return {};
    case Protocol::Mirabox:
      return {
          crt(m, {'D', 'I', 'S'}), crt(m, {'C', 'L', 'E', 0x00, 0x00, 0x00, 0xFF}),
          crt(m, {'S', 'T', 'P'})};
    default:
      return {};
  }
}

std::vector<Report> closeReports(const Model& m)
{
  switch(m.protocol)
  {
    case Protocol::ElgatoGen1: {
      std::vector<uint8_t> r(17, 0);
      r[0] = 0x0B;
      r[1] = 0x63;
      return {{.feature = true, .data = std::move(r)}};
    }
    case Protocol::ElgatoGen2: {
      std::vector<uint8_t> r(32, 0);
      r[0] = 0x03;
      r[1] = 0x02;
      return {{.feature = true, .data = std::move(r)}};
    }
    case Protocol::Mirabox:
      return {crt(m, {'C', 'L', 'E', 0x00, 0x00, 'D', 'C'}), crt(m, {'H', 'A', 'N'})};
  }
  return {};
}

std::vector<Report> flushReports(const Model& m)
{
  if(m.protocol == Protocol::Mirabox)
    return {crt(m, {'S', 'T', 'P'})};
  return {};
}

int keepAliveInterval(const Model& m) noexcept
{
  return m.protocol == Protocol::Mirabox ? 10000 : 0;
}

std::vector<Report> keepAliveReports(const Model& m)
{
  if(m.protocol == Protocol::Mirabox)
    return {crt(m, {'C', 'O', 'N', 'N', 'E', 'C', 'T'})};
  return {};
}

StringReport serialReport(const Model& m) noexcept
{
  switch(m.protocol)
  {
    case Protocol::ElgatoGen1:
      return {0x03, m.productId == 0x0063 ? 17 : 32, 5};
    case Protocol::ElgatoGen2:
      return {0x06, 32, 2};
    default:
      return {0, 0, 0};
  }
}

StringReport firmwareReport(const Model& m) noexcept
{
  switch(m.protocol)
  {
    case Protocol::ElgatoGen1:
      return {0x04, 17, 5};
    case Protocol::ElgatoGen2:
      return {0x05, 32, 6};
    default:
      return {0, 0, 0};
  }
}
}
