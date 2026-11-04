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
class QLineEdit;
class QSlider;
class QTabBar;
class QToolButton;
namespace score
{
struct DocumentContext;
}

namespace StreamDeck
{
class ButtonEditor;
class DeckDevice;
class DeckView;

/**
 * The Stream Deck panel: pick a deck of the document, lay out its pages,
 * set what each key shows and does, and try it from the emulator.
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

private:
  void build();
  void rescanDevices();
  void setDevice(DeckDevice* dev);
  void bindEngine();
  void reload();

  void refreshStatus();
  void refreshPages();
  void refreshFaces();
  void refreshInspector();

  void commit(const Layout& l);
  void commitSettings(const SpecificSettings& s);
  QStringList pageNames() const;
  int keyCount() const;
  bool isLive() const;

  void selectPage(int page);
  void onSelected(int face);
  void onPressed(int face, bool down);
  void onMove(int from, int to, bool copy);
  void onAddress(int key, const QString& address);
  void onImage(int key, const QString& path);
  void onContextMenu(int face, QPoint global);
  void onButtonEdited();
  void onDialEdited();

  void addPage();
  void removePage();
  void duplicatePage();
  void movePage(int delta);

  const score::DocumentContext* m_ctx{};
  QPointer<DeckDevice> m_device;
  QMetaObject::Connection m_engineConnections[3];
  QList<QMetaObject::Connection> m_deviceConnections;

  SpecificSettings m_settings;
  int m_page{};
  int m_selected{-1};
  std::optional<Button> m_clipboard;

  QWidget* m_content{};
  QComboBox* m_devices{};
  QLabel* m_status{};
  QToolButton* m_test{};
  QSlider* m_brightness{};
  QTabBar* m_tabs{};
  QLineEdit* m_pageName{};
  QCheckBox* m_pageTexture{};
  DeckView* m_view{};
  ButtonEditor* m_inspector{};
  QTimer m_repaint;
  ImageCache m_images;
};
}
