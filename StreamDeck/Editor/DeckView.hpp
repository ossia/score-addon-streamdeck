#pragma once
#include <StreamDeck/Model.hpp>

#include <QImage>
#include <QWidget>

#include <verdigris>

namespace StreamDeck
{
/**
 * The keys of a deck, drawn as they look. Faces are the keys, then one per
 * dial (its strip segment and knob).
 *
 * Editing: a click selects, a drag moves a key (copies it with Ctrl), files
 * and addresses can be dropped on a key. In test mode, clicks press the keys
 * and the mouse wheel turns the dials.
 */
class DeckView final : public QWidget
{
  W_OBJECT(DeckView)
public:
  explicit DeckView(QWidget* parent = nullptr);

  void setModel(const Model* m);
  void setFaces(std::vector<QImage> faces);
  void setPressed(std::vector<bool> pressed);
  void setSelected(int face);
  int selected() const noexcept { return m_selected; }
  void setTestMode(bool t);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

  void selectedChanged(int face) W_SIGNAL(selectedChanged, face);
  void pressed(int face, bool down) W_SIGNAL(pressed, face, down);
  void dialTurned(int dial, int delta) W_SIGNAL(dialTurned, dial, delta);
  void moveRequested(int from, int to, bool copy) W_SIGNAL(moveRequested, from, to, copy);
  void fileDropped(int face, QString path) W_SIGNAL(fileDropped, face, path);
  void addressDropped(int face, QString address) W_SIGNAL(addressDropped, face, address);
  void contextMenuRequested(int face, QPoint global)
      W_SIGNAL(contextMenuRequested, face, global);

protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void wheelEvent(QWheelEvent*) override;
  void contextMenuEvent(QContextMenuEvent*) override;
  void dragEnterEvent(QDragEnterEvent*) override;
  void dragMoveEvent(QDragMoveEvent*) override;
  void dropEvent(QDropEvent*) override;

private:
  struct Geometry
  {
    std::vector<QRectF> faces;
    std::vector<QRectF> knobs;
    QRectF body;
  };
  Geometry geometry() const;
  int faceAt(QPointF p) const;

  const Model* m_model{};
  std::vector<QImage> m_faces;
  std::vector<bool> m_pressed;
  int m_selected{-1};
  int m_dropTarget{-1};
  int m_testPressed{-1};
  bool m_test{};
  QPointF m_dragStart;
  int m_dragFace{-1};
};
}
