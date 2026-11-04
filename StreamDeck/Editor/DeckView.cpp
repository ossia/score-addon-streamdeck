#include "DeckView.hpp"

#include <State/MessageListSerialization.hpp>

#include <Device/Node/DeviceNode.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>

#include <score/serialization/MimeVisitor.hpp>

#include <QApplication>
#include <QContextMenuEvent>
#include <QDrag>
#include <QFileInfo>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>

#include <wobjectimpl.h>
W_OBJECT_IMPL(StreamDeck::DeckView)

namespace StreamDeck
{
namespace
{
constexpr const char* key_mime = "application/x-score-streamdeck-key";

std::optional<QString> droppedAddress(const QMimeData& mime)
{
  if(mime.hasFormat(score::mime::nodelist()))
  {
    Mime<Device::FreeNodeList>::Deserializer des{mime};
    if(auto res = Device::addressOfDroppedNodes(des.deserialize(), {}))
      return res->address.address.toString();
  }
  else if(mime.hasFormat(score::mime::messagelist()))
  {
    Mime<State::MessageList>::Deserializer des{mime};
    State::MessageList ml = des.deserialize();
    if(!ml.empty())
      return ml[0].address.address.toString();
  }
  return std::nullopt;
}

bool isImage(const QString& path)
{
  static const QStringList ext{"png", "jpg", "jpeg", "bmp", "gif", "svg", "webp"};
  return ext.contains(QFileInfo{path}.suffix().toLower());
}
}

DeckView::DeckView(QWidget* parent)
    : QWidget{parent}
{
  setAcceptDrops(true);
  setMouseTracking(false);
  setFocusPolicy(Qt::StrongFocus);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void DeckView::setModel(const Model* m)
{
  m_model = m;
  m_selected = -1;
  updateGeometry();
  update();
}

void DeckView::setFaces(std::vector<QImage> faces)
{
  m_faces = std::move(faces);
  update();
}

void DeckView::setPressed(std::vector<bool> pressed)
{
  if(pressed == m_pressed)
    return;
  m_pressed = std::move(pressed);
  update();
}

void DeckView::setSelected(int face)
{
  if(face == m_selected)
    return;
  m_selected = face;
  update();
}

void DeckView::setTestMode(bool t)
{
  m_test = t;
  setCursor(t ? Qt::PointingHandCursor : Qt::ArrowCursor);
  update();
}

namespace
{
struct Proportions
{
  double aspect{1.};
  double gap{0.2};
  double keysW{}, keysH{};
  double stripH{};
  double knobH{};
  double width{}, height{};
};

Proportions proportions(const Model& m)
{
  Proportions p;
  if(m.keyWidth > 0 && m.keyHeight > 0)
    p.aspect = double(m.keyWidth) / m.keyHeight;
  const double cols = std::max(1, m.columns);
  const double rows = std::max(1, m.rows);
  p.keysW = cols * p.aspect + (cols - 1) * p.gap;
  p.keysH = rows + (rows - 1) * p.gap;
  if(m.hasLcd() && m.encoders > 0)
    p.stripH = std::max(0.3, p.keysW * m.lcdHeight / double(m.lcdWidth));
  if(m.encoders > 0)
    p.knobH = 0.75;
  p.width = p.keysW + 2 * p.gap;
  p.height = p.keysH + 2 * p.gap + (p.stripH > 0 ? p.stripH + p.gap : 0.)
             + (p.knobH > 0 ? p.knobH + p.gap : 0.);
  return p;
}
}

QSize DeckView::sizeHint() const
{
  if(!m_model)
    return {420, 260};
  const auto p = proportions(*m_model);
  const double k = 84;
  return QSize(p.width * k, p.height * k);
}

QSize DeckView::minimumSizeHint() const
{
  return {200, 120};
}

DeckView::Geometry DeckView::geometry() const
{
  Geometry g;
  if(!m_model)
    return g;

  const auto p = proportions(*m_model);
  const double k = std::min(width() / p.width, height() / p.height);
  const double gap = k * p.gap;
  const double kw = k * p.aspect;

  const QPointF origin{(width() - p.width * k) / 2., (height() - p.height * k) / 2.};
  g.body = QRectF{origin, QSizeF{p.width * k, p.height * k}};

  for(int r = 0; r < m_model->rows; r++)
    for(int c = 0; c < m_model->columns; c++)
      g.faces.emplace_back(
          origin.x() + gap + c * (kw + gap), origin.y() + gap + r * (k + gap), kw, k);

  if(m_model->encoders > 0)
  {
    double y = origin.y() + gap + p.keysH * k + gap;
    const double stripW = p.keysW * k;
    const double segW = stripW / m_model->encoders;
    if(p.stripH > 0)
    {
      for(int i = 0; i < m_model->encoders; i++)
        g.faces.emplace_back(origin.x() + gap + i * segW, y, segW, p.stripH * k);
      y += p.stripH * k + gap;
    }
    const double d = p.knobH * k;
    for(int i = 0; i < m_model->encoders; i++)
      g.knobs.emplace_back(origin.x() + gap + i * segW + (segW - d) / 2., y, d, d);
  }
  return g;
}

int DeckView::faceAt(QPointF p) const
{
  const auto g = geometry();
  for(int i = 0; i < int(g.faces.size()); i++)
    if(g.faces[i].contains(p))
      return i;
  for(int i = 0; i < int(g.knobs.size()); i++)
    if(g.knobs[i].contains(p))
      return m_model->keyCount() + i;
  return -1;
}

void DeckView::paintEvent(QPaintEvent*)
{
  QPainter p{this};
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  if(!m_model)
  {
    p.setPen(palette().color(QPalette::PlaceholderText));
    p.drawText(rect(), Qt::AlignCenter, tr("No Stream Deck"));
    return;
  }

  const auto g = geometry();
  p.setPen(Qt::NoPen);
  p.setBrush(QColor{0x14, 0x14, 0x16});
  p.drawRoundedRect(g.body, 10, 10);

  const QColor accent{0xf0, 0xa0, 0x20};
  for(int i = 0; i < int(g.faces.size()); i++)
  {
    const auto r = g.faces[i];
    QPainterPath clip;
    clip.addRoundedRect(r, std::min(r.width(), r.height()) * 0.08, std::min(r.width(), r.height()) * 0.08);
    p.save();
    p.setClipPath(clip);
    if(i < int(m_faces.size()) && !m_faces[i].isNull())
      p.drawImage(r, m_faces[i]);
    else
      p.fillRect(r, QColor{0x2a, 0x2a, 0x2e});
    if(i < int(m_pressed.size()) && m_pressed[i])
      p.fillRect(r, QColor{255, 255, 255, 60});
    p.restore();

    p.setBrush(Qt::NoBrush);
    if(i == m_selected)
      p.setPen(QPen{accent, 3});
    else if(i == m_dropTarget)
      p.setPen(QPen{accent, 2, Qt::DashLine});
    else
      p.setPen(QPen{QColor{0x40, 0x40, 0x46}, 1});
    p.drawRoundedRect(r, std::min(r.width(), r.height()) * 0.08, std::min(r.width(), r.height()) * 0.08);
  }

  for(int i = 0; i < int(g.knobs.size()); i++)
  {
    const auto r = g.knobs[i];
    QRadialGradient grad{r.center(), r.width() / 2};
    grad.setColorAt(0, QColor{0x50, 0x50, 0x56});
    grad.setColorAt(1, QColor{0x26, 0x26, 0x2a});
    p.setBrush(grad);
    p.setPen(QPen{
        m_model->keyCount() + i == m_selected ? accent : QColor{0x40, 0x40, 0x46},
        m_model->keyCount() + i == m_selected ? 3. : 1.});
    p.drawEllipse(r);
  }

  if(m_test)
  {
    p.setPen(accent);
    p.drawText(g.body.adjusted(8, 2, -8, -2), Qt::AlignTop | Qt::AlignRight, tr("TEST"));
  }
}

void DeckView::mousePressEvent(QMouseEvent* e)
{
  const int f = faceAt(e->position());
  if(e->button() != Qt::LeftButton)
    return;

  if(m_test)
  {
    if(f >= 0)
    {
      m_testPressed = f;
      pressed(f, true);
    }
    return;
  }

  if(f != m_selected)
  {
    m_selected = f;
    update();
    selectedChanged(f);
  }
  m_dragStart = e->position();
  m_dragFace = f;
}

void DeckView::mouseMoveEvent(QMouseEvent* e)
{
  if(m_test || m_dragFace < 0 || !m_model || m_dragFace >= m_model->keyCount())
    return;
  if((e->position() - m_dragStart).manhattanLength() < QApplication::startDragDistance())
    return;

  auto drag = new QDrag{this};
  auto mime = new QMimeData;
  mime->setData(key_mime, QByteArray::number(m_dragFace));
  drag->setMimeData(mime);
  if(m_dragFace < int(m_faces.size()) && !m_faces[m_dragFace].isNull())
    drag->setPixmap(QPixmap::fromImage(m_faces[m_dragFace].scaled(48, 48)));
  m_dragFace = -1;
  drag->exec(Qt::MoveAction | Qt::CopyAction, Qt::MoveAction);
}

void DeckView::mouseReleaseEvent(QMouseEvent* e)
{
  m_dragFace = -1;
  if(m_test && m_testPressed >= 0)
  {
    pressed(m_testPressed, false);
    m_testPressed = -1;
  }
}

void DeckView::wheelEvent(QWheelEvent* e)
{
  if(!m_test || !m_model)
    return;
  const int f = faceAt(e->position());
  if(f < m_model->keyCount())
    return;
  const int steps = e->angleDelta().y() / 120;
  if(steps != 0)
    dialTurned(f - m_model->keyCount(), steps);
  e->accept();
}

void DeckView::contextMenuEvent(QContextMenuEvent* e)
{
  if(m_test)
    return;
  const int f = faceAt(e->pos());
  if(f < 0)
    return;
  if(f != m_selected)
  {
    m_selected = f;
    update();
    selectedChanged(f);
  }
  contextMenuRequested(f, e->globalPos());
}

void DeckView::dragEnterEvent(QDragEnterEvent* e)
{
  const auto& mime = *e->mimeData();
  if(mime.hasFormat(key_mime) || mime.hasUrls() || mime.hasFormat(score::mime::nodelist())
     || mime.hasFormat(score::mime::messagelist()))
    e->acceptProposedAction();
}

void DeckView::dragMoveEvent(QDragMoveEvent* e)
{
  const int f = faceAt(e->position());
  const bool ok = m_model && f >= 0 && f < m_model->keyCount();
  if(f != m_dropTarget)
  {
    m_dropTarget = ok ? f : -1;
    update();
  }
  if(!ok)
  {
    e->ignore();
    return;
  }
  if(e->mimeData()->hasFormat(key_mime))
    e->setDropAction(
        e->modifiers() & Qt::ControlModifier ? Qt::CopyAction : Qt::MoveAction);
  e->accept();
}

void DeckView::dropEvent(QDropEvent* e)
{
  const int f = m_dropTarget;
  m_dropTarget = -1;
  update();
  if(f < 0)
    return;

  const auto& mime = *e->mimeData();
  if(mime.hasFormat(key_mime))
  {
    const int from = mime.data(key_mime).toInt();
    if(from != f)
      moveRequested(from, f, e->modifiers() & Qt::ControlModifier);
    e->acceptProposedAction();
    return;
  }

  if(auto addr = droppedAddress(mime))
  {
    addressDropped(f, *addr);
    e->acceptProposedAction();
    return;
  }

  for(const auto& url : mime.urls())
  {
    if(url.isLocalFile() && isImage(url.toLocalFile()))
    {
      fileDropped(f, url.toLocalFile());
      e->acceptProposedAction();
      return;
    }
  }
}
}
