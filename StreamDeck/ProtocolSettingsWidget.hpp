#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <verdigris>

class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;

namespace StreamDeck
{
class ProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
  W_OBJECT(ProtocolSettingsWidget)
public:
  ProtocolSettingsWidget(QWidget* parent = nullptr);
  ~ProtocolSettingsWidget() override;

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void rescan();
  void updateDetails();

  QLineEdit* m_name{};
  QComboBox* m_decks{};
  QComboBox* m_mode{};
  QSpinBox* m_brightness{};
  QDoubleSpinBox* m_rate{};
  QLabel* m_details{};
  Device::DeviceSettings m_settings;
};
}
