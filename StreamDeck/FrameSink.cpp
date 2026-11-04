#include "FrameSink.hpp"

#include <StreamDeck/Deck.hpp>

namespace StreamDeck
{
void FrameSink::setDeck(std::shared_ptr<Deck> deck)
{
  std::lock_guard lock{m_mutex};
  m_deck = std::move(deck);
}

void FrameSink::submit(const uint8_t* rgba, int width, int height, int bytesPerLine)
{
  QImage frame
      = QImage{rgba, width, height, bytesPerLine, QImage::Format_RGBA8888}.copy();
  std::shared_ptr<Deck> deck;
  {
    std::lock_guard lock{m_mutex};
    m_last = frame;
    deck = m_deck;
  }
  if(deck)
    deck->submitFrame(frame);
}

QImage FrameSink::lastFrame() const
{
  std::lock_guard lock{m_mutex};
  return m_last;
}
}
