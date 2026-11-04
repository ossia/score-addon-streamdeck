#pragma once
#include <QImage>

#include <memory>
#include <mutex>

namespace StreamDeck
{
class Deck;

//! Where the device's texture goes: the deck when one is plugged, and the
//! last frame, which the editor shows.
class FrameSink
{
public:
  void setDeck(std::shared_ptr<Deck> deck);
  void submit(const uint8_t* rgba, int width, int height, int bytesPerLine);
  QImage lastFrame() const;

private:
  mutable std::mutex m_mutex;
  std::shared_ptr<Deck> m_deck;
  QImage m_last;
};
}
