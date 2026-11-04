#pragma once
#include <StreamDeck/Layout.hpp>
#include <StreamDeck/Model.hpp>
#include <StreamDeck/Render.hpp>

#include <ossia/protocols/hid/hid_device.hpp>

#include <QImage>
#include <QRect>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace StreamDeck
{
//! Keys, then one face per segment of the touch strip.
int faceCount(const Model& m) noexcept;
QSize faceSize(const Model& m, int face) noexcept;

//! The size of the texture the keys of a model are cut from, and where each
//! face sits in it.
QSize canvasSize(const Model& m) noexcept;
QRect tileRect(const Model& m, int face) noexcept;

/**
 * An open Stream Deck.
 *
 * Faces, the texture mask and texture frames can be set from any thread; a
 * worker thread renders and uploads what changed. Input callbacks are called
 * from the HID reader thread.
 */
class Deck
{
public:
  struct Callbacks
  {
    std::function<void(int key, bool pressed)> key;
    std::function<void(int touchKey, bool pressed)> touchKey;
    std::function<void(int encoder, bool pressed)> encoderPress;
    std::function<void(int encoder, int delta)> encoderTurn;
    std::function<void(InputEvent::Kind, int x, int y, int x2, int y2)> touch;
    std::function<void()> unplugged;
  };

  Deck(const Model& model, const ossia::net::hid_device_configuration& conf);
  Deck(const Deck&) = delete;
  Deck& operator=(const Deck&) = delete;
  ~Deck();

  void start(Callbacks cb, int brightness);
  //! Stops the threads; no callback is called once this returns.
  void stop();
  bool unplugged() const noexcept { return m_unplugged; }

  const Model& model() const noexcept { return m_model; }
  const ossia::net::hid_device_info& info() const noexcept { return m_hid.info(); }
  std::string serial();
  std::string firmware();

  void setFace(int index, const Style& f);
  //! Faces for which this is true show their tile of the texture instead.
  void setTextureMask(std::vector<bool> mask);
  void setBrightness(int percent);

  //! RGBA8, top-left origin, of canvasSize().
  void submitFrame(const QImage& frame);

private:
  struct FaceState
  {
    Style face;
    bool dirty{true};
    bool texture{};
  };

  void onInput(std::span<const uint8_t> report);
  void workerLoop();
  void upload(int index, const QImage& img);
  std::string readString(StringReport r);

  const Model& m_model;
  ossia::net::hid_device m_hid;
  Callbacks m_callbacks;

  std::vector<int> m_keyState;
  std::vector<int> m_encoderState;

  std::mutex m_mutex;
  std::condition_variable m_cv;
  std::vector<FaceState> m_faces;
  std::optional<int> m_pendingBrightness;
  QImage m_pendingFrame;
  bool m_stop{};
  std::atomic_bool m_unplugged{false};
  std::thread m_worker;

  ImageCache m_images;
  std::vector<QImage> m_lastTiles;
};
}
