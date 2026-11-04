#include "Device.hpp"

#include <Gfx/GfxApplicationPlugin.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>

#include <StreamDeck/ApplicationPlugin.hpp>
#include <StreamDeck/Deck.hpp>
#include <StreamDeck/Engine.hpp>
#include <StreamDeck/FrameSink.hpp>
#include <StreamDeck/OutputNode.hpp>
#include <StreamDeck/Protocol.hpp>
#include <StreamDeck/RemoteControl.hpp>

#include <QDebug>
#include <QMenu>
#include <QPointer>

#include <wobjectimpl.h>
W_OBJECT_IMPL(StreamDeck::DeckDevice)

namespace StreamDeck
{
namespace
{
ossia::net::hid_device_configuration toConfiguration(const SpecificSettings& s)
{
  return {
      .path = s.path.toStdString(),
      .serial = s.serial.toStdString(),
      .vendor_id = uint16_t(modelFor(s).vendorId),
      .product_id = uint16_t(modelFor(s).productId),
      .usage_page = modelFor(s).usagePage};
}
}

const Model& modelFor(const SpecificSettings& s)
{
  if(auto m = findModel(uint16_t(s.vendorId), uint16_t(s.productId)))
    return *m;
  return *findModel(elgato_vendor_id, 0x0080);
}

DeckDevice::DeckDevice(
    const ::Device::DeviceSettings& settings, const score::DocumentContext& ctx)
    : GfxOutputDevice{settings, ctx}
{
  m_capas.canSerialize = false;
  m_capas.canListen = true;
  m_capas.hasCallbacks = true;

  connect(
      this, &DeckDevice::unplugged, this, &DeckDevice::onUnplugged,
      Qt::QueuedConnection);

  m_replug.setInterval(1000);
  connect(&m_replug, &QTimer::timeout, this, &DeckDevice::attachHardware);
}

DeckDevice::~DeckDevice()
{
  m_replug.stop();
  if(m_hardware)
    m_hardware->stop();
  m_remote.reset();
}

ossia::net::device_base* DeckDevice::getDevice() const
{
  return m_dev.get();
}

void DeckDevice::setupContextMenu(QMenu& menu) const
{
  auto act = menu.addAction(tr("Edit the layout..."));
  connect(act, &QAction::triggered, this, [name = m_settings.name] {
    if(auto plug = score::GUIAppContext().findGuiApplicationPlugin<ApplicationPlugin>())
      plug->showEditor(name);
  });
}

SpecificSettings DeckDevice::specificSettings() const
{
  return m_settings.deviceSpecificSettings.value<SpecificSettings>();
}

bool DeckDevice::reconnect()
{
  disconnect();

  auto plug = m_ctx.findPlugin<Gfx::DocumentPlugin>();
  if(!plug)
    return false;

  const auto set = specificSettings();
  const Model& model = modelFor(set);

  m_engine = std::make_unique<Engine>(m_ctx, model);
  m_engine->setLayout(set.layout);
  m_engine->setBrightness(set.brightness);
  m_sink = std::make_shared<FrameSink>();

  auto node = new OutputNode{m_sink, canvasSize(model), set.rate};
  auto proto = std::make_unique<deck_protocol>(plug->exec, *m_engine);
  auto protocol = proto.get();
  m_dev = std::make_unique<deck_device>(
      std::move(proto), node, m_settings.name.toStdString());
  protocol->updatePages(int(m_engine->layout().pages.size()));

  m_engine->onKey = [protocol](int k, bool p) { protocol->notifyKey(k, p); };
  m_engine->onButton
      = [protocol](int pg, int k, bool p) { protocol->notifyButton(pg, k, p); };
  m_engine->onPage = [protocol](int pg) { protocol->notifyPage(pg); };
  m_engine->deckParameter = [protocol](const QString& path) {
    return protocol->deckParameter(path.toStdString());
  };
  m_engine->writeDeck = [protocol](ossia::net::parameter_base* p, const ossia::value& v) {
    protocol->write(p, v);
  };
  updateDeckAddresses();

  setupRemoteControl(set.mode == SpecificSettings::RemoteControl);

  deviceChanged(nullptr, m_dev.get());
  attachHardware();
  return connected();
}

void DeckDevice::disconnect()
{
  m_replug.stop();
  detachHardware();
  m_remote.reset();

  GfxOutputDevice::disconnect();
  auto prev = std::move(m_dev);
  m_dev = {};
  deviceChanged(prev.get(), nullptr);
  prev.reset();
  m_sink.reset();
  if(m_engine)
  {
    m_engine->shutdown();
    m_engine.release()->deleteLater();
  }
}

void DeckDevice::setupRemoteControl(bool on)
{
  if(!m_engine)
    return;
  if(on && !m_remote)
  {
    m_remote = std::make_unique<RemoteControl>(m_ctx, *m_engine);
    m_engine->setRemoteControl(m_remote.get());
  }
  else if(!on && m_remote)
  {
    m_engine->setRemoteControl(nullptr);
    m_remote.reset();
  }
}

void DeckDevice::applySettings(const SpecificSettings& s)
{
  const auto old = specificSettings();
  m_settings.deviceSpecificSettings = QVariant::fromValue(s);

  if(!old.sameDeck(s) || !m_engine || !m_dev)
  {
    reconnect();
    namespaceUpdated();
    settingsApplied();
    return;
  }

  m_engine->setLayout(s.layout);
  m_engine->setBrightness(s.brightness);
  m_dev->protocol().notifyBrightness(s.brightness);
  setupRemoteControl(s.mode == SpecificSettings::RemoteControl);
  const bool pages = m_dev->protocol().updatePages(int(m_engine->layout().pages.size()));
  if(updateDeckAddresses() || pages)
    namespaceUpdated();
  settingsApplied();
}

bool DeckDevice::updateDeckAddresses()
{
  if(!m_dev || !m_engine)
    return false;
  std::vector<std::pair<std::string, ossia::val_type>> addrs;
  for(const auto& [path, type] : m_engine->layout().deckAddresses())
    addrs.emplace_back(path.toStdString(), type);
  return m_dev->protocol().updateDeckAddresses(addrs);
}

void DeckDevice::attachHardware()
{
  if(m_hardware || !m_engine || !m_dev)
  {
    m_replug.stop();
    return;
  }

  const auto set = specificSettings();
  const Model& model = m_engine->model();
  auto conf = toConfiguration(set);
  if(!ossia::net::hid_find_device(conf))
  {
    m_replug.start();
    return;
  }

  std::shared_ptr<Deck> deck;
  try
  {
    deck = std::make_shared<Deck>(model, conf);
  }
  catch(const std::exception& e)
  {
    qDebug() << "Stream Deck:" << e.what();
    m_replug.start();
    return;
  }
  m_replug.stop();

  auto engine = QPointer<Engine>{m_engine.get()};
  auto onMain = [engine](auto f) {
    if(auto e = engine.data())
      QMetaObject::invokeMethod(e, [e, f = std::move(f)] { f(*e); }, Qt::QueuedConnection);
  };
  auto protocol = &m_dev->protocol();

  deck->start(
      {.key = [onMain](int k, bool p) { onMain([k, p](Engine& e) { e.press(k, p); }); },
       .touchKey = [protocol](int k, bool p) { protocol->notifyTouchKey(k, p); },
       .encoderPress =
           [protocol, onMain](int i, bool p) {
    protocol->notifyEncoderPress(i, p);
    onMain([i, p](Engine& e) { e.pressDial(i, p); });
  },
       .encoderTurn =
           [protocol, onMain](int i, int d) {
    protocol->notifyEncoderTurn(i, d);
    onMain([i, d](Engine& e) { e.turnDial(i, d); });
  },
       .touch = [protocol](InputEvent::Kind k, int x, int y, int x2,
                           int y2) { protocol->notifyTouch(k, x, y, x2, y2); },
       .unplugged = [this] { unplugged(); }},
      m_engine->brightness());

  protocol->setInfo(deck->serial(), deck->firmware());
  m_hardware = deck;
  m_engine->setDeck(deck);
  m_sink->setDeck(deck);
  hardwareChanged();
}

void DeckDevice::detachHardware()
{
  if(!m_hardware)
    return;
  m_hardware->stop();
  if(m_sink)
    m_sink->setDeck(nullptr);
  if(m_engine)
    m_engine->setDeck(nullptr);
  m_hardware.reset();
  hardwareChanged();
}

void DeckDevice::onUnplugged()
{
  if(!m_hardware || !m_hardware->unplugged())
    return;
  detachHardware();
  m_replug.start();
}
}
