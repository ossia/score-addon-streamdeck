#pragma once
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/plugins/panel/PanelDelegateFactory.hpp>

namespace StreamDeck
{
class Editor;

class PanelDelegate final : public score::PanelDelegate
{
public:
  explicit PanelDelegate(const score::GUIApplicationContext& ctx);
  ~PanelDelegate() override;

  QWidget* widget() override;
  const score::PanelStatus& defaultPanelStatus() const override;

private:
  void on_modelChanged(score::MaybeDocument oldm, score::MaybeDocument newm) override;

  Editor* m_widget{};
};

class PanelFactory final : public score::PanelDelegateFactory
{
  SCORE_CONCRETE("2ae2626d-d413-45b9-afe4-b1be60219173")

  std::unique_ptr<score::PanelDelegate>
  make(const score::GUIApplicationContext& ctx) override;
};
}
