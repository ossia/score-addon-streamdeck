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
  //! Mini: BMP images, 17-byte feature reports, key states from byte 1.
  Gen1,
  //! Everything since the Original v2: JPEG images, 32-byte feature reports.
  Gen2,
};

enum class ImageFormat : uint8_t
{
  JPEG,
  BMP,
};

struct Model
{
  uint16_t productId{};
  std::string_view name;
  Protocol protocol{Protocol::Gen2};

  int columns{};
  int rows{};
  int keyWidth{};
  int keyHeight{};
  ImageFormat format{ImageFormat::JPEG};

  //! Applied to a key image, in this order, before it is encoded.
  int rotation{};
  bool flipX{};
  bool flipY{};

  int encoders{};
  int touchKeys{};
  int lcdWidth{};
  int lcdHeight{};

  //! Whether this model was checked against real hardware.
  bool verified{};

  int keyCount() const noexcept { return columns * rows; }
  bool hasDisplay() const noexcept { return keyWidth > 0; }
  bool hasLcd() const noexcept { return lcdWidth > 0; }
};

std::span<const Model> models() noexcept;
const Model* findModel(uint16_t productId) noexcept;

//! What a key / dial / touch strip input report means.
struct InputEvent
{
  enum Kind : uint8_t
  {
    Keys,
    EncoderPress,
    EncoderTurn,
    TouchShort,
    TouchLong,
    TouchSwipe,
    Unknown
  } kind{Unknown};

  //! Keys: one state per key, then per touch key. EncoderPress: one state per
  //! encoder. EncoderTurn: one signed delta per encoder.
  std::vector<int> values;

  //! Touch events, in LCD pixels. Swipe: x, y to x2, y2.
  int x{}, y{}, x2{}, y2{};
};

InputEvent parseInput(const Model& m, std::span<const uint8_t> report);

//! The output reports that upload an encoded key image.
std::vector<std::vector<uint8_t>>
keyImageReports(const Model& m, int key, std::span<const uint8_t> image);

//! The output reports that upload an encoded image to a region of the LCD strip.
std::vector<std::vector<uint8_t>> lcdImageReports(
    const Model& m, int x, int y, int w, int h, std::span<const uint8_t> image);

std::vector<uint8_t> brightnessReport(const Model& m, int percent);
std::vector<uint8_t> resetReport(const Model& m);

//! Feature report ids and offsets of the strings they hold.
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
