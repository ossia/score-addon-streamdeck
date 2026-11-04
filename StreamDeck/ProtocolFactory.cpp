#include "ProtocolFactory.hpp"

#include <Device/Protocol/DeviceSettings.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/protocols/hid/hid_device.hpp>

#include <StreamDeck/Device.hpp>
#include <StreamDeck/Model.hpp>
#include <StreamDeck/ProtocolSettingsWidget.hpp>
#include <StreamDeck/SpecificSettings.hpp>

#include <QObject>
#include <QTimer>
#include <QUrl>

namespace StreamDeck
{
Device::DeviceSettings deckSettings(const ossia::net::hid_device_info& info)
{
  Device::DeviceSettings s;
  s.protocol = ProtocolFactory::static_concreteKey();
  s.name = "streamdeck";

  SpecificSettings specif;
  specif.path = QString::fromStdString(info.path);
  specif.serial = QString::fromStdString(info.serial);
  specif.vendorId = info.vendor_id;
  specif.productId = info.product_id;
  s.deviceSpecificSettings = QVariant::fromValue(specif);
  return s;
}

QString deckLabel(const ossia::net::hid_device_info& info)
{
  const auto* m = findModel(info.vendor_id, info.product_id);
  QString name = m ? QString::fromUtf8(m->name.data(), m->name.size())
                   : QString::fromStdString(info.product);
  if(!info.serial.empty())
    name += QStringLiteral(" (%1)").arg(QString::fromStdString(info.serial));
  return name;
}

std::vector<ossia::net::hid_device_info> connectedDecks()
{
  std::vector<ossia::net::hid_device_info> res;
  for(auto& d : ossia::net::hid_enumerate(0, 0))
    if(auto m = findModel(d.vendor_id, d.product_id))
      if((m->usagePage == 0 || d.usage_page == 0 || d.usage_page == m->usagePage)
         && ossia::none_of(res, [&](const auto& o) { return o.path == d.path; }))
        res.push_back(std::move(d));
  return res;
}

namespace
{
class Enumerator final : public Device::DeviceEnumerator
{
public:
  Enumerator()
  {
    m_timer.setInterval(1000);
    QObject::connect(&m_timer, &QTimer::timeout, this, [this] { rescan(); });
    m_timer.start();
  }

  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
  {
    m_current = scan();
    for(auto& [name, set] : m_current)
      f(name, set);
  }

private:
  using device_list = std::vector<std::pair<QString, Device::DeviceSettings>>;
  static device_list scan()
  {
    device_list res;
    for(const auto& d : connectedDecks())
      res.emplace_back(deckLabel(d), deckSettings(d));
    return res;
  }

  void rescan()
  {
    if(const auto count = ossia::net::hid_device_change_count(); count == m_changeCount)
      return;
    else
      m_changeCount = count;

    auto next = scan();
    auto has = [](const device_list& l, const QString& n) {
      return ossia::any_of(l, [&n](const auto& p) { return p.first == n; });
    };
    for(auto& [name, set] : m_current)
      if(!has(next, name))
        deviceRemoved(name);
    for(auto& [name, set] : next)
      if(!has(m_current, name))
        deviceAdded(name, set);
    m_current = std::move(next);
  }

  QTimer m_timer;
  uint32_t m_changeCount{};
  mutable device_list m_current;
};
}

QString ProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Stream Deck");
}

QString ProtocolFactory::category() const noexcept
{
  return StandardCategories::hardware;
}

QUrl ProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/streamdeck-device.html");
}

Device::DeviceEnumerators
ProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  return {{"Devices", new Enumerator}};
}

Device::DeviceInterface* ProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new DeckDevice{settings, ctx};
}

const Device::DeviceSettings& ProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "streamdeck";
    SpecificSettings specif;
    if(auto decks = connectedDecks(); !decks.empty())
    {
      specif.path = QString::fromStdString(decks.front().path);
      specif.serial = QString::fromStdString(decks.front().serial);
      specif.vendorId = decks.front().vendor_id;
      specif.productId = decks.front().product_id;
    }
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* ProtocolFactory::makeSettingsWidget()
{
  return new ProtocolSettingsWidget;
}

QVariant ProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<SpecificSettings>(visitor);
}

void ProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<SpecificSettings>(data, visitor);
}

bool ProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  if(a.protocol != b.protocol)
    return true;
  const auto sa = a.deviceSpecificSettings.value<SpecificSettings>();
  const auto sb = b.deviceSpecificSettings.value<SpecificSettings>();
  if(!sa.serial.isEmpty() || !sb.serial.isEmpty())
    return sa.serial != sb.serial;
  return sa.path != sb.path;
}
}
