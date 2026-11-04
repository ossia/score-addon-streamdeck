#include "Model.hpp"

#include <algorithm>

namespace StreamDeck
{
namespace
{
// clang-format off
constexpr Model model_table[] = {
  {.productId = 0x0080, .name = "Stream Deck MK.2", .columns = 5, .rows = 3,
   .keyWidth = 72, .keyHeight = 72, .flipX = true, .flipY = true, .verified = true},
  {.productId = 0x00a5, .name = "Stream Deck MK.2 (scissor keys)", .columns = 5, .rows = 3,
   .keyWidth = 72, .keyHeight = 72, .flipX = true, .flipY = true},
  {.productId = 0x006d, .name = "Stream Deck (v2)", .columns = 5, .rows = 3,
   .keyWidth = 72, .keyHeight = 72, .flipX = true, .flipY = true},
  {.productId = 0x006c, .name = "Stream Deck XL", .columns = 8, .rows = 4,
   .keyWidth = 96, .keyHeight = 96, .flipX = true, .flipY = true},
  {.productId = 0x008f, .name = "Stream Deck XL (v2)", .columns = 8, .rows = 4,
   .keyWidth = 96, .keyHeight = 96, .flipX = true, .flipY = true},
  {.productId = 0x0084, .name = "Stream Deck +", .columns = 4, .rows = 2,
   .keyWidth = 120, .keyHeight = 120, .encoders = 4, .lcdWidth = 800, .lcdHeight = 100},
  {.productId = 0x009a, .name = "Stream Deck Neo", .columns = 4, .rows = 2,
   .keyWidth = 96, .keyHeight = 96, .flipX = true, .flipY = true, .touchKeys = 2},
  {.productId = 0x0086, .name = "Stream Deck Pedal", .columns = 3, .rows = 1},
  {.productId = 0x0063, .name = "Stream Deck Mini", .protocol = Protocol::Gen1,
   .columns = 3, .rows = 2, .keyWidth = 80, .keyHeight = 80,
   .format = ImageFormat::BMP, .rotation = 270, .flipY = true},
  {.productId = 0x0090, .name = "Stream Deck Mini (v2)", .protocol = Protocol::Gen1,
   .columns = 3, .rows = 2, .keyWidth = 80, .keyHeight = 80,
   .format = ImageFormat::BMP, .rotation = 270, .flipY = true},
};
// clang-format on

constexpr std::size_t image_report_size = 1024;

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
}

std::span<const Model> models() noexcept
{
  return model_table;
}

const Model* findModel(uint16_t productId) noexcept
{
  for(const auto& m : model_table)
    if(m.productId == productId)
      return &m;
  return nullptr;
}

int inputReportSize(const Model& m) noexcept
{
  return 1024;
}

InputEvent parseInput(const Model& m, std::span<const uint8_t> report)
{
  InputEvent ev;
  if(report.empty())
    return ev;

  auto keys = [&](std::size_t offset, int count) {
    ev.kind = InputEvent::Keys;
    ev.values.resize(count);
    for(int i = 0; i < count; i++)
      ev.values[i] = offset + i < report.size() ? (report[offset + i] != 0) : 0;
  };

  if(m.protocol == Protocol::Gen1)
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
      if(report.size() < std::size_t(5 + m.encoders))
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

std::vector<std::vector<uint8_t>>
keyImageReports(const Model& m, int key, std::span<const uint8_t> image)
{
  std::vector<std::vector<uint8_t>> reports;
  const std::size_t header = m.protocol == Protocol::Gen1 ? 16 : 8;
  const std::size_t payload = image_report_size - header;

  std::size_t sent = 0;
  int page = 0;
  do
  {
    const std::size_t len = std::min(payload, image.size() - sent);
    const bool last = sent + len == image.size();

    std::vector<uint8_t> r(image_report_size, 0);
    r[0] = 0x02;
    if(m.protocol == Protocol::Gen1)
    {
      r[1] = 0x01;
      r[2] = uint8_t(page);
      r[4] = last;
      r[5] = uint8_t(key + 1);
    }
    else
    {
      r[1] = 0x07;
      r[2] = uint8_t(key);
      r[3] = last;
      r[4] = lo(len);
      r[5] = hi(len);
      r[6] = lo(page);
      r[7] = hi(page);
    }
    std::copy_n(image.begin() + sent, len, r.begin() + header);
    reports.push_back(std::move(r));

    sent += len;
    page++;
  } while(sent < image.size());

  return reports;
}

std::vector<std::vector<uint8_t>> lcdImageReports(
    const Model& m, int x, int y, int w, int h, std::span<const uint8_t> image)
{
  std::vector<std::vector<uint8_t>> reports;
  if(!m.hasLcd())
    return reports;

  constexpr std::size_t header = 16;
  constexpr std::size_t payload = image_report_size - header;

  std::size_t sent = 0;
  int page = 0;
  do
  {
    const std::size_t len = std::min(payload, image.size() - sent);
    const bool last = sent + len == image.size();

    std::vector<uint8_t> r(image_report_size, 0);
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
    reports.push_back(std::move(r));

    sent += len;
    page++;
  } while(sent < image.size());

  return reports;
}

std::vector<uint8_t> brightnessReport(const Model& m, int percent)
{
  const auto p = uint8_t(std::clamp(percent, 0, 100));
  if(m.protocol == Protocol::Gen1)
  {
    std::vector<uint8_t> r(17, 0);
    r[0] = 0x05;
    r[1] = 0x55;
    r[2] = 0xaa;
    r[3] = 0xd1;
    r[4] = 0x01;
    r[5] = p;
    return r;
  }
  std::vector<uint8_t> r(32, 0);
  r[0] = 0x03;
  r[1] = 0x08;
  r[2] = p;
  return r;
}

std::vector<uint8_t> resetReport(const Model& m)
{
  if(m.protocol == Protocol::Gen1)
  {
    std::vector<uint8_t> r(17, 0);
    r[0] = 0x0B;
    r[1] = 0x63;
    return r;
  }
  std::vector<uint8_t> r(32, 0);
  r[0] = 0x03;
  r[1] = 0x02;
  return r;
}

StringReport serialReport(const Model& m) noexcept
{
  if(m.protocol == Protocol::Gen1)
    return {0x03, 17, 5};
  return {0x06, 32, 2};
}

StringReport firmwareReport(const Model& m) noexcept
{
  if(m.protocol == Protocol::Gen1)
    return {0x04, 17, 5};
  return {0x05, 32, 6};
}
}
