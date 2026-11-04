#include "ProtocolSettingsWidget.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <score/widgets/HelpInteraction.hpp>

#include <ossia/protocols/hid/hid_device.hpp>

#include <StreamDeck/Deck.hpp>
#include <StreamDeck/Model.hpp>
#include <StreamDeck/ProtocolFactory.hpp>
#include <StreamDeck/SpecificSettings.hpp>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

#include <wobjectimpl.h>
W_OBJECT_IMPL(StreamDeck::ProtocolSettingsWidget)

namespace StreamDeck
{
QString deckLabel(const ossia::net::hid_device_info& info);
std::vector<ossia::net::hid_device_info> connectedDecks();

ProtocolSettingsWidget::ProtocolSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget{parent}
{
  m_name = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_name);
  m_name->setText("streamdeck");

  m_decks = new QComboBox{this};
  auto refresh = new QPushButton{tr("Rescan"), this};
  auto row = new QHBoxLayout;
  row->addWidget(m_decks, 1);
  row->addWidget(refresh);

  m_mode = new QComboBox{this};
  m_mode->addItem(tr("Buttons"), int(SpecificSettings::Buttons));
  m_mode->addItem(tr("Remote control"), int(SpecificSettings::RemoteControl));
  score::setHelp(
      m_mode, tr("Buttons: the keys are laid out in the Stream Deck panel, with "
                 "pages, actions and feedbacks.\n"
                 "Remote control: the bottom row is the transport, the other keys "
                 "and the dials show the controls of the selected process."));

  m_brightness = new QSpinBox{this};
  m_brightness->setRange(0, 100);
  m_brightness->setSuffix(" %");
  m_brightness->setValue(70);

  m_rate = new QDoubleSpinBox{this};
  m_rate->setRange(1., 60.);
  m_rate->setSuffix(" fps");
  m_rate->setValue(30.);
  score::setHelp(m_rate, tr("How often a texture sent to the device is rendered."));

  m_details = new QLabel{this};
  m_details->setWordWrap(true);
  m_details->setTextInteractionFlags(Qt::TextSelectableByMouse);

  auto layout = new QFormLayout;
  layout->addRow(tr("Name"), m_name);
  layout->addRow(tr("Device"), row);
  layout->addRow(tr("Mode"), m_mode);
  layout->addRow(tr("Brightness"), m_brightness);
  layout->addRow(tr("Texture rate"), m_rate);
  layout->addRow(m_details);
  setLayout(layout);

  connect(refresh, &QPushButton::clicked, this, &ProtocolSettingsWidget::rescan);
  connect(m_decks, &QComboBox::currentIndexChanged, this, [this](int idx) {
    if(idx < 0)
      return;
    auto set = m_settings.deviceSpecificSettings.value<SpecificSettings>();
    const auto info = m_decks->itemData(idx).toStringList();
    set.path = info.value(0);
    set.serial = info.value(1);
    set.productId = info.value(2).toInt();
    m_settings.deviceSpecificSettings = QVariant::fromValue(set);
    updateDetails();
  });

  setSettings(ProtocolFactory{}.defaultSettings());
}

ProtocolSettingsWidget::~ProtocolSettingsWidget() { }

void ProtocolSettingsWidget::rescan()
{
  const auto set = m_settings.deviceSpecificSettings.value<SpecificSettings>();
  QSignalBlocker block{m_decks};
  m_decks->clear();
  int selected = -1;
  for(const auto& d : connectedDecks())
  {
    const auto path = QString::fromStdString(d.path);
    const auto serial = QString::fromStdString(d.serial);
    if((!set.serial.isEmpty() && serial == set.serial) || path == set.path)
      selected = m_decks->count();
    m_decks->addItem(
        deckLabel(d), QStringList{path, serial, QString::number(d.product_id)});
  }
  if(selected < 0 && !set.path.isEmpty())
  {
    selected = m_decks->count();
    m_decks->addItem(
        tr("%1 (not connected)").arg(set.serial.isEmpty() ? set.path : set.serial),
        QStringList{set.path, set.serial, QString::number(set.productId)});
  }
  m_decks->setCurrentIndex(selected);
  updateDetails();
}

void ProtocolSettingsWidget::updateDetails()
{
  const auto set = m_settings.deviceSpecificSettings.value<SpecificSettings>();
  const Model* m = findModel(uint16_t(set.productId));
  if(!m)
  {
    m_details->setText(tr("No Stream Deck connected."));
    return;
  }

  QString txt = tr("%1 keys (%2 × %3)").arg(m->keyCount()).arg(m->columns).arg(m->rows);
  if(m->hasDisplay())
    txt += tr(", %1 × %2 px each").arg(m->keyWidth).arg(m->keyHeight);
  if(m->encoders)
    txt += tr(", %1 dials").arg(m->encoders);
  if(m->hasLcd())
    txt += tr(", %1 × %2 px touch strip").arg(m->lcdWidth).arg(m->lcdHeight);
  if(const auto canvas = canvasSize(*m); !canvas.isEmpty())
    txt += QStringLiteral("\n")
           + tr("Texture: %1 × %2 px").arg(canvas.width()).arg(canvas.height());
  if(!m->verified)
    txt += QStringLiteral("\n") + tr("This model has not been tested with score yet.");
  m_details->setText(txt);
}

Device::DeviceSettings ProtocolSettingsWidget::getSettings() const
{
  auto s = m_settings;
  s.name = m_name->text();
  s.protocol = ProtocolFactory::static_concreteKey();
  auto set = s.deviceSpecificSettings.value<SpecificSettings>();
  set.mode = m_mode->currentData().toInt();
  set.brightness = m_brightness->value();
  set.rate = m_rate->value();
  s.deviceSpecificSettings = QVariant::fromValue(set);
  return s;
}

void ProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_settings = settings;
  m_name->setText(settings.name);
  const auto set = settings.deviceSpecificSettings.value<SpecificSettings>();
  m_mode->setCurrentIndex(std::max(0, m_mode->findData(set.mode)));
  m_brightness->setValue(set.brightness);
  m_rate->setValue(set.rate);
  rescan();
}
}
