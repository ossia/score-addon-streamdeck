#pragma once
#include <Gfx/GfxDevice.hpp>

#include <StreamDeck/SpecificSettings.hpp>

#include <QTimer>

#include <memory>

#include <verdigris>

namespace StreamDeck
{
class Deck;
class Engine;
class FrameSink;
class RemoteControl;
class deck_device;
struct Model;

/**
 * A Stream Deck in the document. It exists, with its tree and its layout
 * running, whether or not the deck is plugged: the deck is attached when it
 * shows up and detached when it goes away.
 */
class DeckDevice final : public Gfx::GfxOutputDevice
{
  W_OBJECT(DeckDevice)
public:
  DeckDevice(const ::Device::DeviceSettings& settings, const score::DocumentContext& ctx);
  ~DeckDevice() override;

  bool reconnect() override;
  void disconnect() override;
  ossia::net::device_base* getDevice() const override;
  void setupContextMenu(QMenu& menu) const override;

  SpecificSettings specificSettings() const;
  //! Applies new settings without reconnecting when they open the same deck.
  void applySettings(const SpecificSettings& s);

  Engine* engine() const noexcept { return m_engine.get(); }
  FrameSink* frameSink() const noexcept { return m_sink.get(); }
  bool hardwareConnected() const noexcept { return bool(m_hardware); }

  void unplugged() W_SIGNAL(unplugged);
  void hardwareChanged() W_SIGNAL(hardwareChanged);
  void settingsApplied() W_SIGNAL(settingsApplied);

private:
  void attachHardware();
  void detachHardware();
  void onUnplugged();
  void setupRemoteControl(bool on);
  bool updateDeckAddresses();

  std::unique_ptr<Engine> m_engine;
  std::shared_ptr<FrameSink> m_sink;
  std::shared_ptr<Deck> m_hardware;
  std::unique_ptr<RemoteControl> m_remote;
  std::unique_ptr<deck_device> m_dev;
  QTimer m_replug;
};

const Model& modelFor(const SpecificSettings& s);
}
