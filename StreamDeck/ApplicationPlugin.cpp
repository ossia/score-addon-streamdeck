#include "ApplicationPlugin.hpp"

#include <score/actions/MenuManager.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>

#include <StreamDeck/Editor/Editor.hpp>

#include <QAction>
#include <QMainWindow>
#include <QMenu>

namespace StreamDeck
{
ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& ctx)
    : score::GUIApplicationPlugin{ctx}
{
}

ApplicationPlugin::~ApplicationPlugin()
{
  delete m_editor;
}

score::GUIElements ApplicationPlugin::makeGUIElements()
{
  auto act = new QAction{tr("Stream Deck..."), this};
  act->setShortcut(tr("Ctrl+Shift+J"));
  act->setShortcutContext(Qt::ApplicationShortcut);
  act->setToolTip(tr("Lay out the pages and keys of the Stream Decks of the document"));
  connect(act, &QAction::triggered, this, [this] { showEditor(); });
  context.menus.get().at(score::Menus::Windows()).menu()->addAction(act);
  return {};
}

void ApplicationPlugin::showEditor(const QString& device)
{
  if(!m_editor)
    m_editor = new Editor{context.mainWindow};
  auto doc = currentDocument();
  m_editor->setDocument(doc ? &doc->context() : nullptr);
  if(!device.isEmpty())
    m_editor->showDevice(device);
  m_editor->show();
  m_editor->raise();
  m_editor->activateWindow();
}

void ApplicationPlugin::on_documentChanged(score::Document*, score::Document* newdoc)
{
  if(m_editor)
    m_editor->setDocument(newdoc ? &newdoc->context() : nullptr);
}

void ApplicationPlugin::on_closeDocument(score::Document& doc)
{
  if(m_editor && m_editor->document() == &doc.context())
    m_editor->setDocument(nullptr);
}
}
