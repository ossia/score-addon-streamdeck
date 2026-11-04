#pragma once
#include <Gfx/GfxDevice.hpp>

#include <QTimer>

#include <memory>

#include <verdigris>

namespace StreamDeck
{
class Deck;
class RemoteControl;
class deck_device;

class DeckDevice final : public Gfx::GfxOutputDevice
{
  W_OBJECT(DeckDevice)
public:
  DeckDevice(const ::Device::DeviceSettings& settings, const score::DocumentContext& ctx);
  ~DeckDevice() override;

  bool reconnect() override;
  void disconnect() override;
  ossia::net::device_base* getDevice() const override;

  void unplugged() W_SIGNAL(unplugged);

private:
  void onUnplugged();

  std::shared_ptr<Deck> m_deck;
  std::unique_ptr<RemoteControl> m_remote;
  std::unique_ptr<deck_device> m_dev;
  QTimer m_replug;
};
}
