#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <QObject>
#include <QPointer>

namespace StreamDeck
{
class Editor;

//! Owns the editor window, opened from Windows ▸ Stream Deck or a deck's context menu.
class ApplicationPlugin final
    : public QObject
    , public score::GUIApplicationPlugin
{
public:
  explicit ApplicationPlugin(const score::GUIApplicationContext& ctx);
  ~ApplicationPlugin() override;

  //! Opens the editor on a deck of the current document, or the first one.
  void showEditor(const QString& device = {});

private:
  score::GUIElements makeGUIElements() override;
  void on_documentChanged(score::Document* olddoc, score::Document* newdoc) override;
  void on_closeDocument(score::Document& doc) override;

  QPointer<Editor> m_editor;
};
}
