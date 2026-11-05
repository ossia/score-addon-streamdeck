#pragma once
#include <QImage>

#include <memory>
#include <mutex>

namespace StreamDeck
{
class Deck;

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
