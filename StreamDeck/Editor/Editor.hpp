#pragma once
#include <StreamDeck/Render.hpp>
#include <StreamDeck/SpecificSettings.hpp>

#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <verdigris>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QSlider;
class QSplitter;
class QPushButton;
namespace score
{
struct DocumentContext;
}

namespace StreamDeck
{
class ButtonEditor;
class DeckDevice;
class DeckView;
struct Model;

/**
 * The Stream Deck editor window: pick a deck of the document, lay out its
 * pages, set what each key shows and does, and try it without the hardware.
 *
 * Every change is an undoable command, applied to the running device at once.
 */
class Editor final : public QWidget
{
  W_OBJECT(Editor)
public:
  explicit Editor(QWidget* parent = nullptr);
  ~Editor() override;

  void setDocument(const score::DocumentContext* ctx);
  const score::DocumentContext* document() const noexcept { return m_ctx; }
  void showDevice(const QString& name);

protected:
  void closeEvent(QCloseEvent* e) override;

private:
  void build();
  void rescanDevices();
  void setDevice(DeckDevice* dev);
  void bindEngine();
  void reload();

  const Model* model() const;
  int faceCount() const;
  int livePage() const;

  void refreshStatus();
  void refreshPages();
  void refreshFaces();
  void refreshInspector();

  void commit(const Layout& l);
  void commitSettings(const SpecificSettings& s);
  QStringList pageNames() const;

  void selectPage(int page);
  void selectFace(int face);
  void onPressed(int face, bool down);
  void onMove(int from, int to, bool copy);
  void onAddress(int key, const QString& address);
  void onImage(int key, const QString& path);
  void onContextMenu(int face, QPoint global);
  void onButtonEdited();
  void onDialEdited();
  void setButton(int key, const Button& b);

  void addPage();
  void removePage();
  void duplicatePage();
  void movePage(int delta);
  void renamePage(int page, const QString& name);
  void commitPages(Layout l, const std::vector<int>& newIndexOf, int select);
  void showPageOnDeck(int page);

  const score::DocumentContext* m_ctx{};
  QPointer<DeckDevice> m_device;
  QList<QMetaObject::Connection> m_engineConnections;
  QList<QMetaObject::Connection> m_deviceConnections;
  QList<QMetaObject::Connection> m_documentConnections;

  SpecificSettings m_settings;
  int m_page{};
  int m_selected{-1};
  std::optional<Button> m_clipboard;

  QComboBox* m_devices{};
  QLabel* m_status{};
  QPushButton* m_test{};
  QSlider* m_brightness{};
  QListWidget* m_pages{};
  QPushButton* m_pageUp{};
  QPushButton* m_pageDown{};
  QPushButton* m_pageRemove{};
  QCheckBox* m_pageTexture{};
  DeckView* m_view{};
  QLabel* m_hint{};
  ButtonEditor* m_inspector{};
  QSplitter* m_split{};
  QWidget* m_body{};
  QLabel* m_empty{};
  QTimer m_repaint;
  ImageCache m_images;
};
}
