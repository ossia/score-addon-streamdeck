#include "Editor.hpp"

#include <State/Address.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <ossia/network/base/parameter.hpp>

#include <StreamDeck/Commands.hpp>
#include <StreamDeck/Deck.hpp>
#include <StreamDeck/Device.hpp>
#include <StreamDeck/Editor/ButtonEditor.hpp>
#include <StreamDeck/Editor/DeckView.hpp>
#include <StreamDeck/Engine.hpp>
#include <StreamDeck/FrameSink.hpp>

#include <QCheckBox>
#include <QGridLayout>
#include <QCloseEvent>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QSettings>
#include <QSlider>
#include <QSplitter>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include <wobjectimpl.h>
W_OBJECT_IMPL(StreamDeck::Editor)

namespace StreamDeck
{
namespace
{
bool sameSettings(const SpecificSettings& a, const SpecificSettings& b)
{
  return a.sameDeck(b) && a.mode == b.mode && a.brightness == b.brightness
         && a.layout == b.layout;
}

QString lastSegment(const QString& address)
{
  auto a = State::parseAddress(address);
  if(a && !a->path.isEmpty())
    return a->path.back();
  return address;
}

QString jsString(const QString& s)
{
  QString res = s;
  res.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
  res.replace(QStringLiteral("\""), QStringLiteral("\\\""));
  return QStringLiteral("\"") + res + QStringLiteral("\"");
}

Button transportButton(int which)
{
  Button b;
  b.style.textColor = Qt::white;
  switch(which)
  {
    case 0:
      b.style.text = QStringLiteral("▶");
      b.style.background = QColor{0x1b, 0x3d, 0x1e};
      b.press = {{.type = Action::Send, .address = "score:/play", .value = true}};
      b.feedbacks
          = {{.address = "score:/running",
              .condition = Feedback::IsTrue,
              .style = {.background = QColor{0x2e, 0x9d, 0x3a}}}};
      break;
    case 1:
      b.style.text = QStringLiteral("❚❚");
      b.style.background = QColor{0x3d, 0x33, 0x14};
      b.press = {{.type = Action::Send, .address = "score:/play", .value = false}};
      break;
    case 2:
      b.style.text = QStringLiteral("■");
      b.style.background = QColor{0x4a, 0x16, 0x16};
      b.press
          = {{.type = Action::Send, .address = "score:/stop", .value = ossia::impulse{}}};
      break;
  }
  b.active = {.background = QColor{0xe0, 0xe0, 0xe0}, .textColor = Qt::black};
  return b;
}

Button navigationButton(Action::Type t, int page, const QString& name)
{
  Button b;
  b.style.background = QColor{0x1d, 0x3a, 0x5f};
  switch(t)
  {
    case Action::NextPage:
      b.style.text = QObject::tr("page\n▶");
      break;
    case Action::PreviousPage:
      b.style.text = QObject::tr("◀\npage");
      break;
    default:
      b.style.text = name;
      break;
  }
  b.press = {{.type = t, .page = page}};
  b.active = {.background = QColor{0x3f, 0x6e, 0xa8}};
  return b;
}

QPushButton* button(const QString& text, const QString& tip, QWidget* parent)
{
  auto b = new QPushButton{text, parent};
  b->setToolTip(tip);
  return b;
}
}

Editor::Editor(QWidget* parent)
    : QWidget{parent, Qt::Window}
{
  setWindowTitle(tr("Stream Deck"));
  setAttribute(Qt::WA_DeleteOnClose, false);
  m_repaint.setInterval(66);
  connect(&m_repaint, &QTimer::timeout, this, [this] {
    if(isVisible())
      refreshFaces();
  });
  build();

  QSettings set;
  if(!restoreGeometry(set.value("StreamDeck/EditorGeometry").toByteArray()))
    resize(1180, 640);
}

Editor::~Editor() = default;

void Editor::closeEvent(QCloseEvent* e)
{
  QSettings set;
  set.setValue("StreamDeck/EditorGeometry", saveGeometry());
  if(m_test)
    m_test->setChecked(false);
  QWidget::closeEvent(e);
}

void Editor::build()
{
  auto lay = new QVBoxLayout{this};
  lay->setContentsMargins(6, 6, 6, 6);
  lay->setSpacing(6);

  auto top = new QHBoxLayout;
  top->addWidget(new QLabel{tr("Deck"), this});
  m_devices = new QComboBox{this};
  m_devices->setMinimumContentsLength(12);
  top->addWidget(m_devices);
  m_status = new QLabel{this};
  top->addWidget(m_status, 1);

  m_test = new QPushButton{tr("Try it"), this};
  m_test->setCheckable(true);
  m_test->setToolTip(
      tr("Operate the deck from this window: clicking a key presses it, on the page "
         "shown on the deck; the mouse wheel turns the dials."));
  top->addWidget(m_test);
  top->addSpacing(12);
  top->addWidget(new QLabel{tr("Brightness"), this});
  m_brightness = new QSlider{Qt::Horizontal, this};
  m_brightness->setRange(0, 100);
  m_brightness->setFixedWidth(140);
  top->addWidget(m_brightness);
  auto brightnessValue = new QLabel{this};
  brightnessValue->setMinimumWidth(brightnessValue->fontMetrics().horizontalAdvance("100 %"));
  top->addWidget(brightnessValue);
  connect(m_brightness, &QSlider::valueChanged, brightnessValue, [brightnessValue](int v) {
    brightnessValue->setText(QStringLiteral("%1 %").arg(v));
  });
  lay->addLayout(top);

  m_empty = new QLabel{this};
  m_empty->setAlignment(Qt::AlignCenter);
  m_empty->setWordWrap(true);
  lay->addWidget(m_empty, 1);

  m_body = new QWidget{this};
  auto bodyLay = new QHBoxLayout{m_body};
  bodyLay->setContentsMargins(0, 0, 0, 0);
  auto split = m_split = new QSplitter{Qt::Horizontal, m_body};
  bodyLay->addWidget(split);
  lay->addWidget(m_body, 1);

  auto pagesPane = new QWidget{split};
  auto pagesLay = new QVBoxLayout{pagesPane};
  pagesLay->setContentsMargins(0, 0, 0, 0);
  pagesLay->setSpacing(2);
  auto pagesTitle = new QLabel{tr("Pages"), pagesPane};
  auto bold = pagesTitle->font();
  bold.setBold(true);
  pagesTitle->setFont(bold);
  pagesLay->addWidget(pagesTitle);
  m_pages = new QListWidget{pagesPane};
  m_pages->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
  m_pages->setContextMenuPolicy(Qt::CustomContextMenu);
  m_pages->setToolTip(
      tr("The selected page is the one on the deck.\nDouble-click to rename a page."));
  pagesLay->addWidget(m_pages, 1);

  auto pageButtons = new QGridLayout;
  pageButtons->setSpacing(2);
  auto add = button(tr("Add"), tr("Add a page"), pagesPane);
  auto dup = button(tr("Duplicate"), tr("Duplicate the page"), pagesPane);
  m_pageRemove = button(tr("Delete"), tr("Delete the page"), pagesPane);
  m_pageUp = button(tr("Move up"), tr("Move the page up"), pagesPane);
  m_pageDown = button(tr("Move down"), tr("Move the page down"), pagesPane);
  pageButtons->addWidget(add, 0, 0);
  pageButtons->addWidget(dup, 0, 1);
  pageButtons->addWidget(m_pageUp, 1, 0);
  pageButtons->addWidget(m_pageDown, 1, 1);
  pageButtons->addWidget(m_pageRemove, 2, 0, 1, 2);
  pagesLay->addLayout(pageButtons);
  m_pageTexture = new QCheckBox{tr("Video page"), pagesPane};
  m_pageTexture->setToolTip(
      tr("The keys of this page show their part of the texture sent to the device, "
         "instead of their own look. They still run their actions."));
  pagesLay->addWidget(m_pageTexture);

  auto center = new QWidget{split};
  auto centerLay = new QVBoxLayout{center};
  centerLay->setContentsMargins(0, 0, 0, 0);
  m_view = new DeckView{center};
  centerLay->addWidget(m_view, 1);
  m_hint = new QLabel{center};
  m_hint->setWordWrap(true);
  m_hint->setAlignment(Qt::AlignCenter);
  m_hint->setEnabled(false);
  centerLay->addWidget(m_hint);

  split->addWidget(pagesPane);
  split->addWidget(center);
  split->setStretchFactor(0, 0);
  split->setStretchFactor(1, 1);
  split->setCollapsible(1, false);

  connect(m_devices, &QComboBox::currentIndexChanged, this, [this](int i) {
    if(!m_ctx || i < 0)
      return;
    auto& list = m_ctx->plugin<Explorer::DeviceDocumentPlugin>().list();
    setDevice(dynamic_cast<DeckDevice*>(list.findDevice(m_devices->itemText(i))));
  });
  connect(m_test, &QPushButton::toggled, this, [this](bool t) {
    m_test->setText(t ? tr("■ Stop trying") : tr("Try it"));
    m_view->setTestMode(t);
    if(t)
      selectPage(livePage());
    refreshStatus();
  });
  connect(m_brightness, &QSlider::valueChanged, this, [this](int v) {
    if(m_device && m_device->engine())
      m_device->engine()->setBrightness(v);
    if(!m_brightness->isSliderDown())
    {
      auto s = m_settings;
      s.brightness = v;
      commitSettings(s);
    }
  });
  connect(m_brightness, &QSlider::sliderReleased, this, [this] {
    auto s = m_settings;
    s.brightness = m_brightness->value();
    commitSettings(s);
  });

  connect(m_pages, &QListWidget::currentRowChanged, this, [this](int row) {
    if(row < 0 || row == m_page)
      return;
    selectPage(row);
  });
  connect(m_pages, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
    renamePage(m_pages->row(item), item->text().trimmed());
  });
  connect(m_pages, &QListWidget::customContextMenuRequested, this, [this](QPoint p) {
    const int row = m_pages->indexAt(p).row();
    if(row < 0)
      return;
    selectPage(row);
    QMenu menu;
    menu.addAction(tr("Rename"), this, [this, row] {
      if(auto item = m_pages->item(row))
        m_pages->editItem(item);
    });
    menu.addSeparator();
    menu.addAction(tr("Duplicate"), this, &Editor::duplicatePage);
    menu.addAction(tr("Move up"), this, [this] { movePage(-1); })
        ->setEnabled(row > 0);
    menu.addAction(tr("Move down"), this, [this] { movePage(1); })
        ->setEnabled(row + 1 < m_pages->count());
    menu.addSeparator();
    menu.addAction(tr("Delete"), this, &Editor::removePage)
        ->setEnabled(m_pages->count() > 1);
    menu.exec(m_pages->viewport()->mapToGlobal(p));
  });
  connect(add, &QPushButton::clicked, this, &Editor::addPage);
  connect(dup, &QPushButton::clicked, this, &Editor::duplicatePage);
  connect(m_pageRemove, &QPushButton::clicked, this, &Editor::removePage);
  connect(m_pageUp, &QPushButton::clicked, this, [this] { movePage(-1); });
  connect(m_pageDown, &QPushButton::clicked, this, [this] { movePage(1); });
  connect(m_pageTexture, &QCheckBox::toggled, this, [this](bool t) {
    auto l = m_settings.layout;
    if(m_page >= int(l.pages.size()) || l.pages[m_page].texture == t)
      return;
    l.pages[m_page].texture = t;
    commit(l);
  });

  connect(m_view, &DeckView::selectedChanged, this, &Editor::selectFace);
  connect(m_view, &DeckView::pressed, this, &Editor::onPressed);
  connect(m_view, &DeckView::dialTurned, this, [this](int d, int delta) {
    if(m_device && m_device->engine())
      m_device->engine()->turnDial(d, delta);
  });
  connect(m_view, &DeckView::moveRequested, this, &Editor::onMove);
  connect(m_view, &DeckView::addressDropped, this, &Editor::onAddress);
  connect(m_view, &DeckView::fileDropped, this, &Editor::onImage);
  connect(m_view, &DeckView::contextMenuRequested, this, &Editor::onContextMenu);
}

void Editor::setDocument(const score::DocumentContext* ctx)
{
  if(ctx == m_ctx)
    return;
  for(auto& c : m_documentConnections)
    disconnect(c);
  m_documentConnections.clear();

  setDevice(nullptr);
  m_ctx = ctx;
  delete m_inspector;
  m_inspector = nullptr;
  if(m_ctx)
  {
    m_inspector = new ButtonEditor{*m_ctx};
    m_split->addWidget(m_inspector);
    m_split->setStretchFactor(2, 0);
    m_split->setSizes({170, 560, 420});
    connect(m_inspector, &ButtonEditor::buttonChanged, this, &Editor::onButtonEdited);
    connect(m_inspector, &ButtonEditor::dialChanged, this, &Editor::onDialEdited);

    auto& list = m_ctx->plugin<Explorer::DeviceDocumentPlugin>().list();
    auto rescan = [this] { QTimer::singleShot(0, this, &Editor::rescanDevices); };
    m_documentConnections.push_back(
        connect(&list, &Device::DeviceList::deviceAdded, this, rescan));
    m_documentConnections.push_back(
        connect(&list, &Device::DeviceList::deviceRemoved, this, rescan));
  }
  rescanDevices();
}

void Editor::showDevice(const QString& name)
{
  const int idx = m_devices->findText(name);
  if(idx >= 0)
    m_devices->setCurrentIndex(idx);
}

void Editor::rescanDevices()
{
  const QString current = m_device ? m_device->settings().name : m_devices->currentText();

  {
    QSignalBlocker block{m_devices};
    m_devices->clear();
    if(m_ctx)
    {
      auto& list = m_ctx->plugin<Explorer::DeviceDocumentPlugin>().list();
      for(auto dev : list.devices())
        if(dynamic_cast<DeckDevice*>(dev))
          m_devices->addItem(dev->settings().name);
    }
    int idx = m_devices->findText(current);
    if(idx < 0 && m_devices->count() > 0)
      idx = 0;
    m_devices->setCurrentIndex(idx);
  }

  DeckDevice* dev{};
  if(m_ctx && m_devices->currentIndex() >= 0)
  {
    auto& list = m_ctx->plugin<Explorer::DeviceDocumentPlugin>().list();
    dev = dynamic_cast<DeckDevice*>(list.findDevice(m_devices->currentText()));
  }
  setDevice(dev);
}

void Editor::setDevice(DeckDevice* dev)
{
  if(dev == m_device && dev)
    return;
  for(auto& c : m_deviceConnections)
    disconnect(c);
  m_deviceConnections.clear();

  m_device = dev;
  m_selected = -1;
  m_page = 0;

  if(dev)
  {
    setWindowTitle(tr("Stream Deck — %1").arg(dev->settings().name));
    m_deviceConnections.push_back(
        connect(dev, &DeckDevice::settingsApplied, this, &Editor::reload));
    m_deviceConnections.push_back(
        connect(dev, &DeckDevice::hardwareChanged, this, &Editor::refreshStatus));
    m_deviceConnections.push_back(
        connect(dev, &Device::DeviceInterface::deviceChanged, this, [this] {
      QTimer::singleShot(0, this, [this] {
        bindEngine();
        reload();
      });
    }));
  }
  else
  {
    setWindowTitle(tr("Stream Deck"));
  }
  bindEngine();
  reload();
}

void Editor::bindEngine()
{
  for(auto& c : m_engineConnections)
    disconnect(c);
  m_engineConnections.clear();
  auto engine = m_device ? m_device->engine() : nullptr;
  if(!engine)
    return;
  m_engineConnections.push_back(
      connect(engine, &Engine::pageChanged, this, [this](int p) { selectPage(p); }));
}

const Model* Editor::model() const
{
  return m_device ? &modelFor(m_settings) : nullptr;
}

int Editor::faceCount() const
{
  auto m = model();
  return m ? m->keyCount() + m->encoders : 0;
}

int Editor::livePage() const
{
  auto engine = m_device ? m_device->engine() : nullptr;
  return engine ? engine->page() : -1;
}

void Editor::reload()
{
  if(m_device)
  {
    m_settings = m_device->specificSettings();
    const auto& m = modelFor(m_settings);
    m_settings.layout.normalize(m.keyCount(), m.encoders);
  }
  else
  {
    m_settings = {};
  }
  if(const int live = livePage(); live >= 0)
    m_page = live;
  m_page = std::clamp(m_page, 0, std::max(0, int(m_settings.layout.pages.size()) - 1));
  if(m_selected >= faceCount())
    m_selected = -1;

  m_view->setModel(model());
  m_view->setSelected(m_selected);
  {
    QSignalBlocker b{m_brightness};
    m_brightness->setValue(m_settings.brightness);
  }
  refreshStatus();
  refreshPages();
  refreshInspector();
  refreshFaces();
  if(m_device)
    m_repaint.start();
  else
    m_repaint.stop();
}

QStringList Editor::pageNames() const
{
  QStringList names;
  for(int i = 0; i < int(m_settings.layout.pages.size()); i++)
  {
    const auto& n = m_settings.layout.pages[i].name;
    names.push_back(n.isEmpty() ? tr("Page %1").arg(i + 1) : n);
  }
  return names;
}

void Editor::refreshStatus()
{
  const bool hasDevice = bool(m_device);
  const bool buttons = hasDevice && m_settings.mode == SpecificSettings::Buttons;
  m_body->setVisible(hasDevice);
  m_empty->setVisible(!hasDevice);
  m_test->setEnabled(hasDevice);
  m_brightness->setEnabled(hasDevice);
  m_devices->setEnabled(m_devices->count() > 1);

  if(!m_ctx)
  {
    m_empty->setText(tr("No document is open."));
    m_status->clear();
    return;
  }
  if(!hasDevice)
  {
    m_empty->setText(
        tr("This document has no Stream Deck.\n\nAdd one in the Device Explorer: "
           "click +, then Hardware ▸ Stream Deck. The deck does not need to be "
           "plugged in to lay it out."));
    m_status->clear();
    return;
  }

  const auto& m = modelFor(m_settings);
  QString txt = QString::fromUtf8(m.name.data(), m.name.size());
  if(m_device->hardwareConnected())
    txt = QStringLiteral("<span style='color:#5c5'>●</span> ") + txt.toHtmlEscaped()
          + tr(", connected");
  else
    txt = QStringLiteral("<span style='color:#888'>○</span> ") + txt.toHtmlEscaped()
          + tr(", not plugged in: the layout runs without it");
  if(!buttons)
    txt += tr(" · remote control mode");
  m_status->setText(txt);

  if(m_test->isChecked())
    m_hint->setText(
        tr("Trying it: clicking a key presses it on the deck's current page. The "
           "wheel turns the dials."));
  else
    m_hint->setText(
        tr("Click a key to edit it · drag it to move it, with Ctrl to copy it · drop "
           "an address or an image file on a key · right-click for presets"));
}

void Editor::refreshPages()
{
  const auto names = pageNames();
  {
    QSignalBlocker b{m_pages};
    if(m_pages->count() != names.size())
    {
      m_pages->clear();
      for(int i = 0; i < names.size(); i++)
      {
        auto item = new QListWidgetItem{names[i], m_pages};
        item->setFlags(item->flags() | Qt::ItemIsEditable);
      }
    }
    for(int i = 0; i < names.size(); i++)
    {
      auto item = m_pages->item(i);
      if(item->text() != names[i])
        item->setText(names[i]);
    }
    m_pages->setCurrentRow(m_page);
  }

  const int n = int(m_settings.layout.pages.size());
  m_pageUp->setEnabled(m_page > 0);
  m_pageDown->setEnabled(m_page + 1 < n);
  m_pageRemove->setEnabled(n > 1);
  QSignalBlocker b{m_pageTexture};
  m_pageTexture->setChecked(m_page < n && m_settings.layout.pages[m_page].texture);
}

void Editor::refreshFaces()
{
  auto m = model();
  if(!m || m_settings.layout.pages.empty())
    return;
  const int page = std::clamp(m_page, 0, int(m_settings.layout.pages.size()) - 1);
  const auto& layoutPage = m_settings.layout.pages[page];
  auto engine = m_device->engine();
  const bool engineOk = engine && page < int(engine->layout().pages.size());
  const bool live = engineOk && engine->page() == page;

  QImage frame;
  if(live && engine->isTexture(0) && m_device->frameSink())
    frame = m_device->frameSink()->lastFrame();

  const int n = faceCount();
  std::vector<QImage> faces(n);
  for(int i = 0; i < n; i++)
  {
    QSize size = m->hasDisplay() ? faceSize(*m, i) : QSize{};
    if(size.isEmpty())
      size = {72, 72};
    if(live && !frame.isNull() && frame.size() == canvasSize(*m))
    {
      faces[i] = frame.copy(tileRect(*m, i)).scaled(size);
    }
    else if(engineOk)
    {
      faces[i] = renderStyle(engine->preview(page, i), size, m_images);
    }
    else if(i < m->keyCount())
    {
      faces[i] = renderStyle(layoutPage.buttons[i].style, size, m_images);
    }
    else
    {
      const auto& d = layoutPage.dials[i - m->keyCount()];
      auto s = d.style;
      if(s.text.isEmpty() && !d.address.isEmpty())
        s.text = lastSegment(d.address);
      faces[i] = renderStyle(s, size, m_images);
    }
  }
  m_view->setFaces(std::move(faces));

  std::vector<bool> pressed(m->keyCount());
  if(live)
    for(int i = 0; i < m->keyCount(); i++)
      pressed[i] = engine->isPressed(i);
  m_view->setPressed(std::move(pressed));

  if(m_inspector && engine && m_selected >= 0)
    m_inspector->setScriptError(engine->scriptError(page, m_selected));
}

void Editor::refreshInspector()
{
  if(!m_inspector)
    return;
  auto m = model();
  if(!m)
  {
    m_inspector->showMessage({});
    return;
  }
  if(m_settings.mode == SpecificSettings::RemoteControl)
  {
    m_inspector->showMessage(
        tr("This deck is in remote control mode: its keys show the transport and the "
           "controls of the selected process.\n\nSwitch it to Buttons mode in the "
           "device's settings to lay it out here."));
    return;
  }

  const auto& page = m_settings.layout.pages[m_page];
  const auto pages = pageNames();
  if(m_selected >= 0 && m_selected < m->keyCount())
  {
    const int row = m_selected / m->columns + 1;
    const int col = m_selected % m->columns + 1;
    m_inspector->showButton(
        tr("Key %1 · row %2, column %3 · %4")
            .arg(m_selected + 1)
            .arg(row)
            .arg(col)
            .arg(pages.value(m_page)),
        page.buttons[m_selected], pages,
        deckPath(pages.value(m_page) + QStringLiteral("/key") + QString::number(m_selected + 1)));
  }
  else if(m_selected >= m->keyCount() && m_selected < faceCount())
  {
    const int d = m_selected - m->keyCount();
    m_inspector->showDial(
        tr("Dial %1 · %2").arg(d + 1).arg(pages.value(m_page)), page.dials[d], pages,
        deckPath(pages.value(m_page) + QStringLiteral("/dial") + QString::number(d + 1)));
  }
  else
  {
    m_inspector->showMessage(
        tr("Select a key to set what it shows and what it does.\n\n"
           "Drop an address from the Device Explorer on a key to control it, or an "
           "image file to show it."));
  }
}

void Editor::commit(const Layout& l)
{
  auto s = m_settings;
  s.layout = l;
  commitSettings(s);
}

void Editor::commitSettings(const SpecificSettings& s)
{
  if(!m_ctx || !m_device || sameSettings(s, m_settings))
    return;
  CommandDispatcher<> disp{m_ctx->commandStack};
  disp.submit(new EditDeck{m_device->settings().name, m_device->specificSettings(), s});
}

void Editor::selectPage(int page)
{
  const int n = int(m_settings.layout.pages.size());
  m_page = n > 0 ? std::clamp(page, 0, n - 1) : 0;
  showPageOnDeck(m_page);
  refreshPages();
  refreshInspector();
  refreshFaces();
}

void Editor::selectFace(int face)
{
  m_selected = face < faceCount() ? face : -1;
  m_view->setSelected(m_selected);
  refreshInspector();
  refreshFaces();
}

void Editor::showPageOnDeck(int page)
{
  if(m_device && m_device->engine())
    m_device->engine()->setPage(page);
}

void Editor::onPressed(int face, bool down)
{
  auto engine = m_device ? m_device->engine() : nullptr;
  auto m = model();
  if(!engine || !m)
    return;
  if(face < m->keyCount())
    engine->press(face, down);
  else
    engine->pressDial(face - m->keyCount(), down);
  refreshFaces();
}

void Editor::setButton(int key, const Button& b)
{
  auto l = m_settings.layout;
  if(m_page >= int(l.pages.size()) || key < 0 || key >= int(l.pages[m_page].buttons.size()))
    return;
  l.pages[m_page].buttons[key] = b;
  commit(l);
}

void Editor::onMove(int from, int to, bool copy)
{
  auto l = m_settings.layout;
  auto& buttons = l.pages[m_page].buttons;
  if(from < 0 || to < 0 || from >= int(buttons.size()) || to >= int(buttons.size()))
    return;
  if(copy)
    buttons[to] = buttons[from];
  else
    std::swap(buttons[from], buttons[to]);
  m_selected = to;
  m_view->setSelected(to);
  commit(l);
  refreshInspector();
}

void Editor::onAddress(int key, const QString& address)
{
  const auto name = lastSegment(address);
  Button b;
  b.style.background = QColor{0x26, 0x32, 0x38};
  b.active = {.background = QColor{0x60, 0x7d, 0x8b}};

  auto p = m_ctx ? resolveAddress(*m_ctx, address) : nullptr;
  const auto type = p ? p->get_value_type() : ossia::val_type::IMPULSE;
  const auto read = QStringLiteral("Device.read(") + jsString(address) + QStringLiteral(")");
  switch(type)
  {
    case ossia::val_type::BOOL:
      b.style.text = name;
      b.press = {{.type = Action::Toggle, .address = address}};
      b.feedbacks
          = {{.address = address,
              .condition = Feedback::IsTrue,
              .style = {.background = QColor{0x2e, 0x9d, 0x3a}}}};
      break;
    case ossia::val_type::IMPULSE:
      b.style.text = name;
      b.press = {{.type = Action::Send, .address = address, .value = ossia::impulse{}}};
      break;
    case ossia::val_type::FLOAT:
      b.style.text = name + QStringLiteral("\n${Number(") + read
                     + QStringLiteral(").toFixed(2)}");
      b.press = {{.type = Action::Send, .address = address, .value = p->value()}};
      break;
    default:
      b.style.text = name + QStringLiteral("\n${") + read + QStringLiteral("}");
      b.press = {{.type = Action::Send, .address = address, .value = p->value()}};
      break;
  }
  m_selected = key;
  m_view->setSelected(key);
  setButton(key, b);
  refreshInspector();
}

void Editor::onImage(int key, const QString& path)
{
  if(m_page >= int(m_settings.layout.pages.size()))
    return;
  auto b = m_settings.layout.pages[m_page].buttons.at(key);
  b.style.image = path;
  m_selected = key;
  m_view->setSelected(key);
  setButton(key, b);
  refreshInspector();
}

void Editor::onContextMenu(int face, QPoint global)
{
  auto m = model();
  if(!m || face < 0 || face >= m->keyCount())
    return;
  selectFace(face);

  QMenu menu;
  auto set = [this, face](Button b) {
    setButton(face, b);
    refreshInspector();
  };
  const auto current = m_settings.layout.pages[m_page].buttons[face];

  menu.addAction(tr("Copy"), this, [this, current] { m_clipboard = current; });
  menu.addAction(tr("Paste"), this, [this, set] {
    if(m_clipboard)
      set(*m_clipboard);
  })->setEnabled(bool(m_clipboard));
  menu.addAction(tr("Clear"), this, [set] { set(Button{}); })
      ->setEnabled(!current.empty());
  menu.addSeparator();

  const auto names0 = pageNames();
  const auto name = deckPath(
      names0.value(m_page) + QStringLiteral("/key") + QString::number(face + 1));
  auto trigger = menu.addMenu(tr("Trigger"));
  trigger->addAction(tr("Impulse at deck:/actions/%1").arg(name), this, [set, name] {
    Button b;
    b.style.text = name.section(QChar{'/'}, -1);
    b.style.background = QColor{0x26, 0x32, 0x38};
    b.active = {.background = QColor{0x60, 0x7d, 0x8b}};
    b.press = {{.type = Action::Trigger, .address = name}};
    set(b);
  });
  trigger->addAction(tr("On / off at deck:/actions/%1").arg(name), this, [set, name] {
    Button b;
    b.style.text = name.section(QChar{'/'}, -1);
    b.style.background = QColor{0x26, 0x32, 0x38};
    b.active = {.background = QColor{0x2e, 0x9d, 0x3a}};
    b.press = {{.type = Action::Trigger, .address = name, .toggle = true}};
    set(b);
  });

  auto transport = menu.addMenu(tr("Transport"));
  transport->addAction(tr("Play"), this, [set] { set(transportButton(0)); });
  transport->addAction(tr("Pause"), this, [set] { set(transportButton(1)); });
  transport->addAction(tr("Stop"), this, [set] { set(transportButton(2)); });

  auto nav = menu.addMenu(tr("Pages"));
  nav->addAction(tr("Next page"), this, [set] {
    set(navigationButton(Action::NextPage, 0, {}));
  });
  nav->addAction(tr("Previous page"), this, [set] {
    set(navigationButton(Action::PreviousPage, 0, {}));
  });
  nav->addSeparator();
  const auto names = pageNames();
  for(int i = 0; i < names.size(); i++)
    nav->addAction(tr("Go to %1").arg(names[i]), this, [set, i, n = names[i]] {
      set(navigationButton(Action::GotoPage, i, n));
    });

  menu.exec(global);
}

void Editor::onButtonEdited()
{
  auto m = model();
  if(!m || !m_inspector || m_selected < 0 || m_selected >= m->keyCount())
    return;
  setButton(m_selected, m_inspector->button());
}

void Editor::onDialEdited()
{
  auto m = model();
  if(!m || !m_inspector)
    return;
  const int d = m_selected - m->keyCount();
  auto l = m_settings.layout;
  if(m_page >= int(l.pages.size()) || d < 0 || d >= int(l.pages[m_page].dials.size()))
    return;
  l.pages[m_page].dials[d] = m_inspector->dial();
  commit(l);
}

void Editor::commitPages(Layout l, const std::vector<int>& newIndexOf, int select)
{
  auto remap = [&](std::vector<Action>& actions) {
    for(auto& a : actions)
      if(a.type == Action::GotoPage && a.page >= 0 && a.page < int(newIndexOf.size()))
        a.page = std::max(0, newIndexOf[a.page]);
  };
  for(auto& p : l.pages)
  {
    for(auto& b : p.buttons)
    {
      remap(b.press);
      remap(b.release);
    }
    for(auto& d : p.dials)
      remap(d.press);
  }
  commit(l);
  selectPage(select);
}

void Editor::addPage()
{
  auto l = m_settings.layout;
  const int n = int(l.pages.size());
  l.pages.push_back({.name = tr("Page %1").arg(n + 1)});
  commit(l);
  selectPage(n);
}

void Editor::removePage()
{
  auto l = m_settings.layout;
  const int n = int(l.pages.size());
  const int page = m_page;
  if(n <= 1 || page >= n)
    return;
  std::vector<int> idx(n);
  for(int i = 0; i < n; i++)
    idx[i] = i < page ? i : i == page ? -1 : i - 1;
  l.pages.erase(l.pages.begin() + page);
  commitPages(std::move(l), idx, page - 1);
}

void Editor::duplicatePage()
{
  auto l = m_settings.layout;
  const int n = int(l.pages.size());
  const int page = m_page;
  if(page >= n)
    return;
  std::vector<int> idx(n);
  for(int i = 0; i < n; i++)
    idx[i] = i <= page ? i : i + 1;
  auto copy = l.pages[page];
  copy.name = tr("%1 (copy)").arg(pageNames().value(page));
  l.pages.insert(l.pages.begin() + page + 1, std::move(copy));
  commitPages(std::move(l), idx, page + 1);
}

void Editor::movePage(int delta)
{
  auto l = m_settings.layout;
  const int n = int(l.pages.size());
  const int from = m_page;
  const int to = from + delta;
  if(to < 0 || to >= n)
    return;
  std::vector<int> idx(n);
  for(int i = 0; i < n; i++)
    idx[i] = i == from ? to : i == to ? from : i;
  std::swap(l.pages[from], l.pages[to]);
  commitPages(std::move(l), idx, to);
}

void Editor::renamePage(int page, const QString& name)
{
  auto l = m_settings.layout;
  if(page < 0 || page >= int(l.pages.size()) || l.pages[page].name == name)
    return;
  l.pages[page].name = name;
  commit(l);
}
}
