#include "Panel.hpp"

#include <StreamDeck/Panel/Editor.hpp>

namespace StreamDeck
{
PanelDelegate::PanelDelegate(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}
    , m_widget{new Editor}
{
}

PanelDelegate::~PanelDelegate() = default;

QWidget* PanelDelegate::widget()
{
  return m_widget;
}

const score::PanelStatus& PanelDelegate::defaultPanelStatus() const
{
  static const score::PanelStatus status{
      false,
      false,
      Qt::BottomDockWidgetArea,
      0,
      QObject::tr("Stream Deck"),
      "device",
      QObject::tr("Ctrl+Shift+J")};
  return status;
}

void PanelDelegate::on_modelChanged(score::MaybeDocument, score::MaybeDocument newm)
{
  m_widget->setDocument(newm);
}

std::unique_ptr<score::PanelDelegate>
PanelFactory::make(const score::GUIApplicationContext& ctx)
{
  return std::make_unique<PanelDelegate>(ctx);
}
}
