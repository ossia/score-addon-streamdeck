#include "Device.hpp"

#include <Gfx/GfxApplicationPlugin.hpp>

#include <score/document/DocumentContext.hpp>

#include <StreamDeck/Deck.hpp>
#include <StreamDeck/OutputNode.hpp>
#include <StreamDeck/Protocol.hpp>
#include <StreamDeck/RemoteControl.hpp>
#include <StreamDeck/SpecificSettings.hpp>

#include <QDebug>
#include <QPointer>

#include <wobjectimpl.h>
W_OBJECT_IMPL(StreamDeck::DeckDevice)

namespace StreamDeck
{
static ossia::net::hid_device_configuration toConfiguration(const SpecificSettings& s)
{
  return {
      .path = s.path.toStdString(),
      .serial = s.serial.toStdString(),
      .vendor_id = elgato_vendor_id,
      .product_id = uint16_t(s.productId)};
}

DeckDevice::DeckDevice(
    const ::Device::DeviceSettings& settings, const score::DocumentContext& ctx)
    : GfxOutputDevice{settings, ctx}
{
  m_capas.canSerialize = false;
  m_capas.canListen = true;
  m_capas.hasCallbacks = true;

  connect(
      this, &DeckDevice::unplugged, this, &DeckDevice::onUnplugged, Qt::QueuedConnection);

  m_replug.setInterval(1000);
  connect(&m_replug, &QTimer::timeout, this, [this] {
    auto set = this->settings().deviceSpecificSettings.value<SpecificSettings>();
    if(ossia::net::hid_find_device(toConfiguration(set)))
    {
      m_replug.stop();
      if(reconnect())
        namespaceUpdated();
    }
  });
}

DeckDevice::~DeckDevice()
{
  if(m_deck)
    m_deck->stop();
  m_remote.reset();
}

ossia::net::device_base* DeckDevice::getDevice() const
{
  return m_dev.get();
}

bool DeckDevice::reconnect()
{
  disconnect();

  const auto set = m_settings.deviceSpecificSettings.value<SpecificSettings>();
  const Model* model = findModel(uint16_t(set.productId));
  if(!model)
  {
    qDebug() << "Stream Deck: unknown model" << Qt::hex << set.productId;
    return false;
  }

  auto plug = m_ctx.findPlugin<Gfx::DocumentPlugin>();
  if(!plug)
    return false;

  try
  {
    m_deck = std::make_shared<Deck>(*model, toConfiguration(set));
  }
  catch(const std::exception& e)
  {
    qDebug() << "Stream Deck:" << e.what();
    m_replug.start();
    return false;
  }

  const auto canvas = m_deck->canvasSize();
  auto node = new OutputNode{m_deck, canvas, set.rate};
  auto proto = std::make_unique<deck_protocol>(plug->exec, m_deck, set.brightness);
  auto protocol = proto.get();
  m_dev = std::make_unique<deck_device>(
      std::move(proto), node, m_settings.name.toStdString());

  if(set.mode == SpecificSettings::RemoteControl)
    m_remote = std::make_unique<RemoteControl>(m_ctx, m_deck);

  auto remote = QPointer<QObject>{m_remote.get()};
  auto onMain = [remote](auto f) {
    if(remote)
      QMetaObject::invokeMethod(remote.data(), std::move(f), Qt::QueuedConnection);
  };
  auto rc = m_remote.get();

  m_deck->start(
      {.key =
           [protocol, onMain, rc](int k, bool p) {
    protocol->onKey(k, p);
    onMain([rc, k, p] { rc->onKey(k, p); });
  },
       .touchKey = [protocol](int k, bool p) { protocol->onTouchKey(k, p); },
       .encoderPress =
           [protocol, onMain, rc](int e, bool p) {
    protocol->onEncoderPress(e, p);
    onMain([rc, e, p] { rc->onEncoderPress(e, p); });
  },
       .encoderTurn =
           [protocol, onMain, rc](int e, int d) {
    protocol->onEncoderTurn(e, d);
    onMain([rc, e, d] { rc->onEncoderTurn(e, d); });
  },
       .touch = [protocol](InputEvent::Kind k, int x, int y, int x2,
                           int y2) { protocol->onTouch(k, x, y, x2, y2); },
       .unplugged = [this] { unplugged(); }},
      set.brightness);

  deviceChanged(nullptr, m_dev.get());
  return connected();
}

void DeckDevice::disconnect()
{
  if(m_deck)
    m_deck->stop();
  m_remote.reset();

  GfxOutputDevice::disconnect();
  auto prev = std::move(m_dev);
  m_dev = {};
  deviceChanged(prev.get(), nullptr);
  prev.reset();
  m_deck.reset();
}

void DeckDevice::onUnplugged()
{
  if(m_deck && !m_deck->unplugged())
    return;
  disconnect();
  m_replug.start();
}
}
