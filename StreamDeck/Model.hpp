#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace StreamDeck
{
static constexpr uint16_t elgato_vendor_id = 0x0fd9;

enum class Protocol : uint8_t
{
  //! Elgato Mini: BMP images, 17-byte feature reports, key states from byte 1.
  ElgatoGen1,
  //! Elgato, everything since the Original v2: JPEG images, 32-byte feature reports.
  ElgatoGen2,
  //! Mirabox / Ajazz "Stream Dock" and their re-brands: "CRT" commands in
  //! output reports, "ACK" input reports, one key per event.
  Mirabox,
};

enum class ImageFormat : uint8_t
{
  JPEG,
  BMP,
};

//! The input codes of a dial of a Mirabox device.
struct DialCodes
{
  uint8_t left{}, right{}, press{};
};

struct Model
{
  uint16_t vendorId{elgato_vendor_id};
  uint16_t productId{};
  std::string_view name;
  Protocol protocol{Protocol::ElgatoGen2};

  int columns{};
  int rows{};
  int keyWidth{};
  int keyHeight{};
  ImageFormat format{ImageFormat::JPEG};

  //! Applied to a key image, in this order, before it is encoded. Clockwise.
  int rotation{};
  bool flipX{};
  bool flipY{};

  //! A column of smaller keys, with images of this size.
  int smallColumn{-1};
  int smallKeySize{};

  int encoders{};
  //! Keys without a display (Neo touch keys, Mirabox side buttons).
  int touchKeys{};
  int lcdWidth{};
  int lcdHeight{};

  //! Bytes written per output report, report id included.
  int reportSize{1024};
  //! HID usage page of the interface to open; 0 for any.
  uint16_t usagePage{};
  //! Only presses are reported: releases follow them at once.
  bool pressOnly{};

  //! The device's number for each key, row by row; empty when they match.
  std::span<const uint8_t> keyMap;
  //! The code of each key in input reports, when it differs from keyMap.
  std::span<const uint8_t> inputMap;
  //! Mirabox: the image number of each segment of the strip, its tap code,
  //! the input codes of the keys without display and of the dials.
  std::span<const uint8_t> stripMap;
  std::span<const uint8_t> stripTapCodes;
  std::span<const uint8_t> touchKeyCodes;
  std::span<const DialCodes> dialCodes;

  //! Whether this model was checked against real hardware.
  bool verified{};

  int keyCount() const noexcept { return columns * rows; }
  bool hasDisplay() const noexcept { return keyWidth > 0; }
  bool hasLcd() const noexcept { return lcdWidth > 0; }
  int deviceKey(int key) const noexcept { return keyMap.empty() ? key : keyMap[key]; }
};

std::span<const Model> models() noexcept;
const Model* findModel(uint16_t vendorId, uint16_t productId) noexcept;
bool isKnownVendor(uint16_t vendorId) noexcept;

struct Report
{
  bool feature{};
  std::vector<uint8_t> data;
};

//! What a key / dial / touch strip input report means.
struct InputEvent
{
  enum Kind : uint8_t
  {
    //! values: the state of every key, then of every touch key.
    Keys,
    //! values: {key, pressed}, for one key; touch keys follow the keys.
    Key,
    //! values: one state per encoder.
    EncoderPress,
    //! values: one signed delta per encoder.
    EncoderTurn,
    TouchShort,
    TouchLong,
    TouchSwipe,
    Unknown
  } kind{Unknown};

  std::vector<int> values;

  //! A press without its release: the release follows at once.
  bool tap{};

  //! Touch events, in LCD pixels. Swipe: x, y to x2, y2.
  int x{}, y{}, x2{}, y2{};
};

InputEvent parseInput(const Model& m, std::span<const uint8_t> report);

//! Sent once the deck is open.
std::vector<Report> openReports(const Model& m);
//! Sent before the deck is closed.
std::vector<Report> closeReports(const Model& m);

std::vector<Report> keyImageReports(const Model& m, int key, std::span<const uint8_t> image);
std::vector<Report>
stripImageReports(const Model& m, int segment, std::span<const uint8_t> image);
//! Sent after a batch of images, for the deck to show them.
std::vector<Report> flushReports(const Model& m);

std::vector<Report> brightnessReports(const Model& m, int percent);

//! How often the deck must hear from us not to go to sleep (ms); 0 if never.
int keepAliveInterval(const Model& m) noexcept;
std::vector<Report> keepAliveReports(const Model& m);

//! Feature report ids and offsets of the strings they hold; length 0 if none.
struct StringReport
{
  uint8_t id;
  int length;
  int offset;
};
StringReport serialReport(const Model& m) noexcept;
StringReport firmwareReport(const Model& m) noexcept;

int inputReportSize(const Model& m) noexcept;
}
