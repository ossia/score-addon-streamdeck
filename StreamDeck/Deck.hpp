#pragma once
#include <StreamDeck/Model.hpp>

#include <ossia/protocols/hid/hid_device.hpp>

#include <QColor>
#include <QImage>
#include <QRect>
#include <QString>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace StreamDeck
{
//! What a key, or a segment of the LCD strip, shows.
struct Face
{
  QColor background{Qt::black};
  QString text;
  QColor textColor{Qt::white};
  //! Pixels; 0 picks a size that fits.
  int textSize{};
  QString imagePath;
};

//! The size of the texture the keys of a model are cut from.
QSize canvasSize(const Model& m) noexcept;

/**
 * An open Stream Deck.
 *
 * Faces and texture frames can be set from any thread; a worker thread
 * renders and uploads what changed. Input callbacks are called from the HID
 * reader thread.
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

  //! Index: 0..keyCount()-1 for keys, then one per LCD segment.
  int faceCount() const noexcept;
  void setFace(int index, const Face& f);
  template <typename F>
  void updateFace(int index, F&& f)
  {
    std::lock_guard lock{m_mutex};
    if(index < 0 || index >= int(m_faces.size()))
      return;
    f(m_faces[index].face);
    m_faces[index].dirty = true;
    m_cv.notify_one();
  }

  void clear();
  void setBrightness(int percent);

  //! The size of the texture the keys are cut from.
  QSize canvasSize() const noexcept;
  //! RGBA8, top-left origin, of canvasSize().
  void submitFrame(const uint8_t* rgba, int width, int height, int bytesPerLine);

private:
  struct FaceState
  {
    Face face;
    bool dirty{true};
  };

  struct WorkerCache
  {
    QImage image;
    QString path;
    QImage lastTile;
  };

  void onInput(std::span<const uint8_t> report);
  void workerLoop();
  QImage renderFace(const Face& f, WorkerCache& cache, QSize size);
  void upload(int index, const QImage& img);
  QRect tileRect(int index) const noexcept;
  QSize faceSize(int index) const noexcept;
  std::string readString(StringReport r);

  const Model& m_model;
  ossia::net::hid_device m_hid;
  Callbacks m_callbacks;

  std::vector<int> m_keyState;
  std::vector<int> m_encoderState;

  std::mutex m_mutex;
  std::condition_variable m_cv;
  std::vector<FaceState> m_faces;
  std::vector<WorkerCache> m_cache;
  std::optional<int> m_pendingBrightness;
  QImage m_pendingFrame;
  bool m_stop{};
  std::atomic_bool m_unplugged{false};
  std::thread m_worker;
};
}
