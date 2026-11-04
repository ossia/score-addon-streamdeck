#include "Editor.hpp"

#include <State/Address.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <ossia/network/base/parameter.hpp>

#include <StreamDeck/Commands.hpp>
#include <StreamDeck/Deck.hpp>
#include <StreamDeck/Device.hpp>
#include <StreamDeck/Engine.hpp>
#include <StreamDeck/FrameSink.hpp>
#include <StreamDeck/Panel/ButtonEditor.hpp>
#include <StreamDeck/Panel/DeckView.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QSlider>
#include <QSplitter>
#include <QTabBar>
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
}

Editor::Editor(QWidget* parent)
    : QWidget{parent}
{
  auto lay = new QVBoxLayout{this};
  lay->setContentsMargins(0, 0, 0, 0);
  m_repaint.setInterval(66);
  connect(&m_repaint, &QTimer::timeout, this, [this] {
    if(isVisible())
      refreshFaces();
  });
  setDocument(nullptr);
}

Editor::~Editor() = default;

void Editor::setDocument(const score::DocumentContext* ctx)
{
  m_ctx = ctx;
  setDevice(nullptr);
  build();
  if(m_ctx)
  {
    auto& list = m_ctx->plugin<Explorer::DeviceDocumentPlugin>().list();
    connect(&list, &Device::DeviceList::deviceAdded, m_content, [this] {
      QTimer::singleShot(0, this, &Editor::rescanDevices);
    });
    connect(&list, &Device::DeviceList::deviceRemoved, m_content, [this] {
      QTimer::singleShot(0, this, &Editor::rescanDevices);
    });
    rescanDevices();
  }
}

void Editor::build()
{
  delete m_content;
  m_devices = nullptr;
  m_status = nullptr;
  m_test = nullptr;
  m_brightness = nullptr;
  m_tabs = nullptr;
  m_pageName = nullptr;
  m_pageTexture = nullptr;
  m_view = nullptr;
  m_inspector = nullptr;
  m_content = new QWidget{this};
  layout()->addWidget(m_content);
  auto lay = new QVBoxLayout{m_content};
  lay->setContentsMargins(4, 4, 4, 4);
  lay->setSpacing(4);

  if(!m_ctx)
  {
    auto l = new QLabel{tr("No document"), m_content};
    l->setAlignment(Qt::AlignCenter);
    lay->addWidget(l);
    m_repaint.stop();
    return;
  }

  auto top = new QHBoxLayout;
  m_devices = new QComboBox{m_content};
  m_devices->setMinimumContentsLength(12);
  top->addWidget(m_devices);
  m_status = new QLabel{m_content};
  top->addWidget(m_status, 1);

  m_test = new QToolButton{m_content};
  m_test->setText(tr("Test"));
  m_test->setCheckable(true);
  m_test->setToolTip(
      tr("Operate the deck from here: clicks press the keys of the page shown on the "
         "deck, the mouse wheel turns the dials."));
  top->addWidget(m_test);

  top->addWidget(new QLabel{tr("Brightness"), m_content});
  m_brightness = new QSlider{Qt::Horizontal, m_content};
  m_brightness->setRange(0, 100);
  m_brightness->setMinimumWidth(100);
  m_brightness->setMaximumWidth(160);
  top->addWidget(m_brightness);
  lay->addLayout(top);

  auto pages = new QHBoxLayout;
  m_tabs = new QTabBar{m_content};
  m_tabs->setExpanding(false);
  m_tabs->setDrawBase(false);
  m_tabs->setContextMenuPolicy(Qt::CustomContextMenu);
  m_tabs->setToolTip(tr("Double-click a page to show it on the deck"));
  pages->addWidget(m_tabs);
  auto add = new QToolButton{m_content};
  add->setText(QStringLiteral("+"));
  add->setToolTip(tr("Add a page"));
  pages->addWidget(add);
  pages->addStretch(1);
  pages->addWidget(new QLabel{tr("Page"), m_content});
  m_pageName = new QLineEdit{m_content};
  m_pageName->setMaximumWidth(160);
  pages->addWidget(m_pageName);
  m_pageTexture = new QCheckBox{tr("Video"), m_content};
  m_pageTexture->setToolTip(
      tr("The keys of this page show the texture sent to the device instead of "
         "their own look; they still run their actions."));
  pages->addWidget(m_pageTexture);
  auto pageMenu = new QToolButton{m_content};
  pageMenu->setText(QStringLiteral("⋯"));
  pageMenu->setPopupMode(QToolButton::InstantPopup);
  auto menu = new QMenu{pageMenu};
  menu->addAction(tr("Show on the deck"), this, [this] {
    if(m_device && m_device->engine())
      m_device->engine()->setPage(m_page);
  });
  menu->addSeparator();
  menu->addAction(tr("Duplicate page"), this, &Editor::duplicatePage);
  menu->addAction(tr("Move left"), this, [this] { movePage(-1); });
  menu->addAction(tr("Move right"), this, [this] { movePage(1); });
  menu->addSeparator();
  menu->addAction(tr("Delete page"), this, &Editor::removePage);
  pageMenu->setMenu(menu);
  pages->addWidget(pageMenu);
  lay->addLayout(pages);

  auto split = new QSplitter{Qt::Horizontal, m_content};
  m_view = new DeckView{split};
  m_inspector = new ButtonEditor{*m_ctx, split};
  split->addWidget(m_view);
  split->addWidget(m_inspector);
  split->setStretchFactor(0, 3);
  split->setStretchFactor(1, 2);
  lay->addWidget(split, 1);

  connect(m_devices, &QComboBox::currentIndexChanged, this, [this](int i) {
    if(!m_ctx || i < 0)
      return;
    auto& list = m_ctx->plugin<Explorer::DeviceDocumentPlugin>().list();
    setDevice(dynamic_cast<DeckDevice*>(list.findDevice(m_devices->itemText(i))));
  });
  connect(m_test, &QToolButton::toggled, this, [this](bool t) {
    m_view->setTestMode(t);
    if(t && m_device && m_device->engine())
      selectPage(m_device->engine()->page());
  });
  connect(m_brightness, &QSlider::valueChanged, this, [this](int v) {
    if(m_device && m_device->engine())
      m_device->engine()->setBrightness(v);
    if(!m_brightness->isSliderDown() && v != m_settings.brightness)
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

  connect(m_tabs, &QTabBar::currentChanged, this, [this](int i) {
    if(i < 0 || i == m_page)
      return;
    if(m_test->isChecked() && m_device && m_device->engine())
      m_device->engine()->setPage(i);
    selectPage(i);
  });
  connect(m_tabs, &QTabBar::tabBarDoubleClicked, this, [this](int i) {
    if(i >= 0 && m_device && m_device->engine())
      m_device->engine()->setPage(i);
  });
  connect(m_tabs, &QTabBar::customContextMenuRequested, this, [this, menu](QPoint p) {
    const int i = m_tabs->tabAt(p);
    if(i < 0)
      return;
    selectPage(i);
    menu->exec(m_tabs->mapToGlobal(p));
  });
  connect(add, &QToolButton::clicked, this, &Editor::addPage);
  connect(m_pageName, &QLineEdit::editingFinished, this, [this] {
    auto l = m_settings.layout;
    if(m_page >= int(l.pages.size()) || l.pages[m_page].name == m_pageName->text())
      return;
    l.pages[m_page].name = m_pageName->text();
    commit(l);
  });
  connect(m_pageTexture, &QCheckBox::toggled, this, [this](bool t) {
    auto l = m_settings.layout;
    if(m_page >= int(l.pages.size()) || l.pages[m_page].texture == t)
      return;
    l.pages[m_page].texture = t;
    commit(l);
  });

  connect(m_view, &DeckView::selectedChanged, this, &Editor::onSelected);
  connect(m_view, &DeckView::pressed, this, &Editor::onPressed);
  connect(m_view, &DeckView::dialTurned, this, [this](int d, int delta) {
    if(m_device && m_device->engine())
      m_device->engine()->turnDial(d, delta);
  });
  connect(m_view, &DeckView::moveRequested, this, &Editor::onMove);
  connect(m_view, &DeckView::addressDropped, this, &Editor::onAddress);
  connect(m_view, &DeckView::fileDropped, this, &Editor::onImage);
  connect(m_view, &DeckView::contextMenuRequested, this, &Editor::onContextMenu);
  connect(m_inspector, &ButtonEditor::buttonChanged, this, &Editor::onButtonEdited);
  connect(m_inspector, &ButtonEditor::dialChanged, this, &Editor::onDialEdited);

  m_repaint.start();
}

void Editor::rescanDevices()
{
  if(!m_ctx || !m_devices)
    return;
  const QString current = m_device ? m_device->settings().name : m_devices->currentText();

  QSignalBlocker block{m_devices};
  m_devices->clear();
  auto& list = m_ctx->plugin<Explorer::DeviceDocumentPlugin>().list();
  for(auto dev : list.devices())
    if(dynamic_cast<DeckDevice*>(dev))
      m_devices->addItem(dev->settings().name);

  int idx = m_devices->findText(current);
  if(idx < 0 && m_devices->count() > 0)
    idx = 0;
  m_devices->setCurrentIndex(idx);
  setDevice(
      idx >= 0 ? dynamic_cast<DeckDevice*>(list.findDevice(m_devices->itemText(idx)))
               : nullptr);
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
  if(m_view)
    m_view->setSelected(-1);

  if(dev)
  {
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
  bindEngine();
  reload();
}

void Editor::bindEngine()
{
  for(auto& c : m_engineConnections)
    disconnect(c);
  auto engine = m_device ? m_device->engine() : nullptr;
  if(!engine)
    return;
  m_engineConnections[0] = connect(engine, &Engine::pageChanged, this, [this](int p) {
    if(m_test && m_test->isChecked())
      selectPage(p);
    else
      refreshPages();
  });
  m_engineConnections[1] = connect(engine, &Engine::layoutChanged, this, [this] {
    refreshFaces();
  });
}

void Editor::reload()
{
  if(m_device)
  {
    m_settings = m_device->specificSettings();
    const auto& model = modelFor(m_settings);
    m_settings.layout.normalize(model.keyCount(), model.encoders);
    m_page = std::clamp(m_page, 0, int(m_settings.layout.pages.size()) - 1);
  }
  else
  {
    m_settings = {};
    m_page = 0;
  }

  if(!m_view)
    return;
  m_view->setModel(m_device ? &modelFor(m_settings) : nullptr);
  m_view->setSelected(m_selected);
  {
    QSignalBlocker b{m_brightness};
    m_brightness->setValue(m_settings.brightness);
  }
  refreshStatus();
  refreshPages();
  refreshInspector();
  refreshFaces();
}

int Editor::keyCount() const
{
  return m_device ? modelFor(m_settings).keyCount() : 0;
}

bool Editor::isLive() const
{
  return m_device && m_device->engine() && m_device->engine()->page() == m_page;
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
  if(!m_status)
    return;
  const bool enabled = bool(m_device);
  for(auto w : std::initializer_list<QWidget*>{
          m_test, m_brightness, m_tabs, m_pageName, m_pageTexture, m_view})
    w->setEnabled(enabled);

  if(!m_device)
  {
    m_status->setText(
        tr("No Stream Deck in this document: add one in the Device Explorer "
           "(Hardware, Stream Deck)."));
    return;
  }

  const auto& model = modelFor(m_settings);
  QString txt = QString::fromUtf8(model.name.data(), model.name.size());
  if(m_device->hardwareConnected())
    txt = QStringLiteral("● ") + txt;
  else
    txt = QStringLiteral("○ ") + txt + tr(" — not plugged: the layout runs here");
  if(m_settings.mode == SpecificSettings::RemoteControl)
    txt += tr(" — remote control mode");
  m_status->setText(txt);
}

void Editor::refreshPages()
{
  if(!m_tabs)
    return;
  QSignalBlocker b{m_tabs};
  while(m_tabs->count() > 0)
    m_tabs->removeTab(0);
  const auto names = pageNames();
  const int live = m_device && m_device->engine() ? m_device->engine()->page() : -1;
  for(int i = 0; i < names.size(); i++)
    m_tabs->addTab(i == live ? QStringLiteral("▶ ") + names[i] : names[i]);
  m_tabs->setCurrentIndex(m_page);

  const bool hasPage = m_page < int(m_settings.layout.pages.size());
  QSignalBlocker b2{m_pageName}, b3{m_pageTexture};
  m_pageName->setText(hasPage ? m_settings.layout.pages[m_page].name : QString{});
  m_pageTexture->setChecked(hasPage && m_settings.layout.pages[m_page].texture);
}

void Editor::refreshFaces()
{
  if(!m_view || !m_device)
    return;
  const auto& model = modelFor(m_settings);
  const int n = faceCount(model);
  auto engine = m_device->engine();
  const bool live = isLive();
  const auto& page = m_settings.layout.pages[m_page];

  QImage frame;
  if(live && engine && engine->isTexture(0) && m_device->frameSink())
    frame = m_device->frameSink()->lastFrame();

  std::vector<QImage> faces(n);
  for(int i = 0; i < n; i++)
  {
    const auto size = faceSize(model, i);
    if(live && engine)
    {
      if(engine->isTexture(i) && frame.size() == canvasSize(model))
        faces[i] = frame.copy(tileRect(model, i)).scaled(size);
      else
        faces[i] = renderStyle(engine->face(i), size, m_images);
    }
    else if(i < model.keyCount())
    {
      faces[i] = renderStyle(page.buttons[i].style, size, m_images);
    }
    else
    {
      const auto& d = page.dials[i - model.keyCount()];
      auto s = d.style;
      if(s.text.isEmpty() && !d.address.isEmpty())
        s.text = lastSegment(d.address);
      faces[i] = renderStyle(s, size, m_images);
    }
  }
  m_view->setFaces(std::move(faces));

  std::vector<bool> pressed(model.keyCount());
  if(live && engine)
    for(int i = 0; i < model.keyCount(); i++)
      pressed[i] = engine->isPressed(i);
  m_view->setPressed(std::move(pressed));
}

void Editor::refreshInspector()
{
  if(!m_inspector)
    return;
  if(!m_device)
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

  const auto& model = modelFor(m_settings);
  const auto& page = m_settings.layout.pages[m_page];
  const auto pages = pageNames();
  if(m_selected >= 0 && m_selected < model.keyCount())
  {
    const int row = m_selected / model.columns + 1;
    const int col = m_selected % model.columns + 1;
    m_inspector->showButton(
        tr("Key %1 — row %2, column %3 — %4")
            .arg(m_selected + 1)
            .arg(row)
            .arg(col)
            .arg(pages.value(m_page)),
        page.buttons[m_selected], pages);
  }
  else if(m_selected >= model.keyCount() && m_selected < model.keyCount() + model.encoders)
  {
    const int d = m_selected - model.keyCount();
    m_inspector->showDial(
        tr("Dial %1 — %2").arg(d + 1).arg(pages.value(m_page)), page.dials[d], pages);
  }
  else
  {
    m_inspector->showMessage(
        tr("Select a key to set what it shows and does.\n\n"
           "Drop an address from the Device Explorer on a key to control it, or an "
           "image file to show it. Drag keys to move them, with Ctrl to copy them."));
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
  page = std::clamp(page, 0, int(m_settings.layout.pages.size()) - 1);
  m_page = page;
  refreshPages();
  refreshInspector();
  refreshFaces();
}

void Editor::onSelected(int face)
{
  m_selected = face;
  refreshInspector();
}

void Editor::onPressed(int face, bool down)
{
  auto engine = m_device ? m_device->engine() : nullptr;
  if(!engine)
    return;
  const int keys = keyCount();
  if(face < keys)
    engine->press(face, down);
  else
    engine->pressDial(face - keys, down);
  refreshFaces();
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
}

void Editor::onAddress(int key, const QString& address)
{
  auto l = m_settings.layout;
  auto& b = l.pages[m_page].buttons[key];
  const auto name = lastSegment(address);
  b = Button{};
  b.style.background = QColor{0x26, 0x32, 0x38};
  b.active = {.background = QColor{0x60, 0x7d, 0x8b}};

  auto p = m_ctx ? resolveAddress(*m_ctx, address) : nullptr;
  const auto type = p ? p->get_value_type() : ossia::val_type::IMPULSE;
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
    default:
      b.style.text = name + QStringLiteral("\n$(") + address + QStringLiteral(")");
      b.press = {{.type = Action::Send, .address = address, .value = p->value()}};
      break;
  }
  m_selected = key;
  m_view->setSelected(key);
  commit(l);
}

void Editor::onImage(int key, const QString& path)
{
  auto l = m_settings.layout;
  l.pages[m_page].buttons[key].style.image = path;
  m_selected = key;
  m_view->setSelected(key);
  commit(l);
}

void Editor::onContextMenu(int face, QPoint global)
{
  const int keys = keyCount();
  if(face < 0 || face >= keys)
    return;

  QMenu menu;
  auto setButton = [this, face](Button b) {
    auto l = m_settings.layout;
    l.pages[m_page].buttons[face] = std::move(b);
    commit(l);
  };
  const auto& current = m_settings.layout.pages[m_page].buttons[face];

  menu.addAction(tr("Copy"), this, [this, current] { m_clipboard = current; });
  auto paste = menu.addAction(tr("Paste"), this, [this, setButton] {
    if(m_clipboard)
      setButton(*m_clipboard);
  });
  paste->setEnabled(bool(m_clipboard));
  auto clear = menu.addAction(tr("Clear"), this, [setButton] { setButton(Button{}); });
  clear->setEnabled(!current.empty());
  menu.addSeparator();

  auto transport = menu.addMenu(tr("Transport"));
  transport->addAction(tr("Play"), this, [setButton] { setButton(transportButton(0)); });
  transport->addAction(tr("Pause"), this, [setButton] { setButton(transportButton(1)); });
  transport->addAction(tr("Stop"), this, [setButton] { setButton(transportButton(2)); });

  auto nav = menu.addMenu(tr("Pages"));
  nav->addAction(tr("Next page"), this, [setButton] {
    setButton(navigationButton(Action::NextPage, 0, {}));
  });
  nav->addAction(tr("Previous page"), this, [setButton] {
    setButton(navigationButton(Action::PreviousPage, 0, {}));
  });
  nav->addSeparator();
  const auto names = pageNames();
  for(int i = 0; i < names.size(); i++)
    nav->addAction(tr("Go to %1").arg(names[i]), this, [setButton, i, n = names[i]] {
      setButton(navigationButton(Action::GotoPage, i, n));
    });

  menu.exec(global);
}

void Editor::onButtonEdited()
{
  if(m_selected < 0 || m_selected >= keyCount())
    return;
  auto l = m_settings.layout;
  l.pages[m_page].buttons[m_selected] = m_inspector->button();
  commit(l);
}

void Editor::onDialEdited()
{
  const int d = m_selected - keyCount();
  auto l = m_settings.layout;
  if(d < 0 || d >= int(l.pages[m_page].dials.size()))
    return;
  l.pages[m_page].dials[d] = m_inspector->dial();
  commit(l);
}

void Editor::addPage()
{
  auto l = m_settings.layout;
  l.pages.push_back({.name = tr("Page %1").arg(l.pages.size() + 1)});
  m_page = int(l.pages.size()) - 1;
  commit(l);
}

void Editor::removePage()
{
  auto l = m_settings.layout;
  if(l.pages.size() <= 1)
    return;
  l.pages.erase(l.pages.begin() + m_page);
  m_page = std::max(0, m_page - 1);
  commit(l);
}

void Editor::duplicatePage()
{
  auto l = m_settings.layout;
  auto copy = l.pages[m_page];
  copy.name = tr("%1 (copy)").arg(pageNames().value(m_page));
  l.pages.insert(l.pages.begin() + m_page + 1, std::move(copy));
  m_page++;
  commit(l);
}

void Editor::movePage(int delta)
{
  auto l = m_settings.layout;
  const int to = m_page + delta;
  if(to < 0 || to >= int(l.pages.size()))
    return;
  std::swap(l.pages[m_page], l.pages[to]);
  m_page = to;
  commit(l);
}
}
