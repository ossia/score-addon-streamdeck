#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <QObject>
#include <QPointer>

namespace StreamDeck
{
class Editor;

class ApplicationPlugin final
    : public QObject
    , public score::GUIApplicationPlugin
{
public:
  explicit ApplicationPlugin(const score::GUIApplicationContext& ctx);
  ~ApplicationPlugin() override;

  void showEditor(const QString& device = {});

private:
  score::GUIElements makeGUIElements() override;
  void on_documentChanged(score::Document* olddoc, score::Document* newdoc) override;
  void on_closeDocument(score::Document& doc) override;

  QPointer<Editor> m_editor;
};
}
