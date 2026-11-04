#include "Engine.hpp"

#include <State/Address.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/domain/domain_functions.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <StreamDeck/Deck.hpp>
#include <StreamDeck/RemoteControl.hpp>
#include <StreamDeck/Script.hpp>

#include <cmath>

#include <wobjectimpl.h>
W_OBJECT_IMPL(StreamDeck::Engine)

namespace StreamDeck
{
namespace
{
bool isNumeric(const ossia::value& v)
{
  switch(v.get_type())
  {
    case ossia::val_type::INT:
    case ossia::val_type::FLOAT:
    case ossia::val_type::BOOL:
      return true;
    default:
      return false;
  }
}

QString displayValue(const ossia::value& v)
{
  switch(v.get_type())
  {
    case ossia::val_type::FLOAT: {
      const float f = *v.target<float>();
      return QString::number(f, 'f', std::abs(f) >= 100.f ? 0 : 2);
    }
    case ossia::val_type::INT:
      return QString::number(*v.target<int>());
    case ossia::val_type::BOOL:
      return *v.target<bool>() ? QStringLiteral("on") : QStringLiteral("off");
    case ossia::val_type::STRING:
      return QString::fromStdString(*v.target<std::string>());
    case ossia::val_type::IMPULSE:
    case ossia::val_type::NONE:
      return {};
    default:
      return valueToString(v);
  }
}
}

Engine::Engine(const score::DocumentContext& ctx, const Model& model)
    : m_ctx{ctx}
    , m_model{model}
    , m_scripts{std::make_unique<Scripts>(*this)}
{
  m_displayed.resize(::StreamDeck::faceCount(model));
  m_keys.resize(model.keyCount());
  m_layout.normalize(model.keyCount(), model.encoders);

  m_timer.setInterval(50);
  connect(&m_timer, &QTimer::timeout, this, &Engine::refresh);
  m_timer.start();
  refresh();
}

Engine::~Engine() = default;

void Engine::setLayout(Layout l)
{
  l.normalize(m_model.keyCount(), m_model.encoders);
  if(l == m_layout)
    return;
  m_layout = std::move(l);
  const int pages = int(m_layout.pages.size());
  std::erase_if(m_toggles, [pages](const auto& t) { return t.first.first >= pages; });
  std::erase_if(m_overrides, [pages](const auto& t) { return t.first.first >= pages; });
  std::erase_if(m_errors, [pages](const auto& t) { return std::get<0>(t.first) >= pages; });
  for(auto& k : m_keys)
    if(k.page >= pages)
      k.fired = false;

  const bool clamped = m_page >= pages;
  if(clamped)
    m_page = pages - 1;
  applyTextureMask();
  refresh();
  layoutChanged();
  if(clamped)
  {
    if(onPage)
      onPage(m_page);
    pageChanged(m_page);
  }
}

void Engine::setPage(int page)
{
  page = std::clamp(page, 0, int(m_layout.pages.size()) - 1);
  if(page == m_page)
    return;
  m_page = page;
  applyTextureMask();
  refresh();
  if(onPage)
    onPage(m_page);
  pageChanged(m_page);
}

void Engine::shutdown()
{
  deckParameter = {};
  writeDeck = {};
  m_shutdown = true;
  m_timer.stop();
  m_deck.reset();
  m_remote = nullptr;
  onKey = {};
  onButton = {};
  onPage = {};
}

void Engine::nextPage()
{
  setPage((m_page + 1) % int(m_layout.pages.size()));
}

void Engine::previousPage()
{
  const int n = int(m_layout.pages.size());
  setPage((m_page + n - 1) % n);
}

void Engine::setBrightness(int b)
{
  m_brightness = std::clamp(b, 0, 100);
  if(m_deck)
    m_deck->setBrightness(m_brightness);
}

bool Engine::isPressed(int key) const
{
  return key >= 0 && key < int(m_keys.size()) && m_keys[key].down;
}

bool Engine::isTexture(int face) const
{
  if(m_remote)
    return false;
  return m_layout.pages[m_page].texture && face < int(m_displayed.size());
}

void Engine::setDeck(std::shared_ptr<Deck> deck)
{
  m_deck = std::move(deck);
  if(!m_deck)
    return;
  m_deck->setBrightness(m_brightness);
  applyTextureMask();
  for(int i = 0; i < int(m_displayed.size()); i++)
    m_deck->setFace(i, m_displayed[i]);
}

void Engine::setRemoteControl(RemoteControl* rc)
{
  m_remote = rc;
  applyTextureMask();
  refresh();
}

void Engine::display(int face, const Style& s)
{
  if(face < 0 || face >= int(m_displayed.size()))
    return;
  if(m_displayed[face] == s)
    return;
  m_displayed[face] = s;
  if(m_deck)
    m_deck->setFace(face, s);
  faceChanged(face);
}

void Engine::applyTextureMask()
{
  if(m_deck)
    m_deck->setTextureMask(std::vector<bool>(m_displayed.size(), isTexture(0)));
}

void Engine::setOverride(int page, int key, const StyleOverride& o)
{
  auto& cur = m_overrides[{page, key}];
  if(o.background)
    cur.background = o.background->alpha() == 0 ? std::nullopt : o.background;
  if(o.textColor)
    cur.textColor = o.textColor->alpha() == 0 ? std::nullopt : o.textColor;
  if(o.text)
    cur.text = o.text->isEmpty() ? std::nullopt : o.text;
  if(o.image)
    cur.image = o.image->isEmpty() ? std::nullopt : o.image;
  if(page == m_page)
    refresh();
}

void Engine::press(int key, bool down)
{
  if(m_shutdown)
    return;
  if(key < 0 || key >= int(m_keys.size()))
    return;
  auto& k = m_keys[key];
  if(k.down == down)
    return;
  k.down = down;

  if(onKey)
    onKey(key, down);

  if(m_remote)
  {
    m_remote->onKey(key, down);
    return;
  }

  if(down)
    k.page = m_page;
  const int page = k.page;
  if(onButton)
    onButton(page, key, down);

  const int gen = ++k.generation;
  if(page >= int(m_layout.pages.size()))
  {
    k.fired = false;
    refresh();
    return;
  }
  const int hold = m_layout.pages[page].buttons[key].hold;
  if(down)
  {
    k.fired = false;
    if(hold > 0)
    {
      QTimer::singleShot(hold, this, [this, key, gen, page] {
        auto& k = m_keys[key];
        if(k.generation == gen && k.down)
        {
          k.fired = true;
          fire(page, key, true);
        }
      });
    }
    else
    {
      k.fired = true;
      fire(page, key, true);
    }
  }
  else if(k.fired)
  {
    fire(page, key, false);
  }
  refresh();
}

void Engine::fire(int page, int key, bool down)
{
  if(m_shutdown)
    return;
  if(page >= int(m_layout.pages.size()))
    return;
  const auto& b = m_layout.pages[page].buttons[key];
  if(b.mode == Button::Toggle)
  {
    if(!down)
      return;
    bool& on = m_toggles[{page, key}];
    on = !on;
    run(page, key, on ? b.press : b.release);
  }
  else
  {
    run(page, key, down ? b.press : b.release);
  }
  refresh();
}

void Engine::pressDial(int dial, bool down)
{
  if(m_shutdown)
    return;
  if(m_remote)
  {
    m_remote->onEncoderPress(dial, down);
    return;
  }
  if(!down || dial < 0 || dial >= m_model.encoders)
    return;
  run(m_page, m_model.keyCount() + dial, m_layout.pages[m_page].dials[dial].press);
}

void Engine::turnDial(int dial, int delta)
{
  if(m_shutdown)
    return;
  if(m_remote)
  {
    m_remote->onEncoderTurn(dial, delta);
    return;
  }
  if(dial < 0 || dial >= m_model.encoders)
    return;
  const auto& d = m_layout.pages[m_page].dials[dial];
  if(d.address.isEmpty())
    return;
  runNow(
      m_page, m_model.keyCount() + dial,
      {.type = Action::Step, .address = d.address, .step = d.step * delta});
  refresh();
}

void Engine::run(int page, int face, std::vector<Action> actions)
{
  if(m_shutdown)
    return;
  for(const auto& a : actions)
  {
    if(a.delay > 0)
      QTimer::singleShot(a.delay, this, [this, page, face, a] {
        if(m_shutdown)
          return;
        runNow(page, face, a);
        refresh();
      });
    else
      runNow(page, face, a);
  }
}

ossia::net::parameter_base* Engine::resolve(const QString& address) const
{
  return resolveAddress(m_ctx, address);
}

ossia::net::parameter_base*
resolveAddress(const score::DocumentContext& ctx, const QString& address)
{
  auto addr = State::parseAddress(address);
  if(!addr)
    return nullptr;
  auto plug = ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
  if(!plug)
    return nullptr;
  auto dev = plug->list().findDevice(addr->device);
  if(!dev)
    return nullptr;
  auto ossia_dev = dev->getDevice();
  if(!ossia_dev)
    return nullptr;
  auto node = ossia::net::find_node(
      ossia_dev->get_root_node(), QString{QStringLiteral("/") + addr->path.join(QChar{'/'})}.toStdString());
  return node ? node->get_parameter() : nullptr;
}

void Engine::runNow(int page, int face, const Action& a)
{
  if(m_shutdown)
    return;
  switch(a.type)
  {
    case Action::GotoPage:
      setPage(a.page);
      return;
    case Action::NextPage:
      nextPage();
      return;
    case Action::PreviousPage:
      previousPage();
      return;
    case Action::Trigger: {
      auto p = deckParameter ? deckParameter(a.deckAddress()) : nullptr;
      if(p && writeDeck)
        writeDeck(
            p, a.toggle ? ossia::value{!ossia::convert<bool>(p->value())}
                        : ossia::value{ossia::impulse{}});
      return;
    }
    case Action::Script: {
      QString err;
      m_scripts->run(a.script, face, err);
      setError(page, face, 2, err);
      return;
    }
    default:
      break;
  }

  auto p = resolve(a.address);
  if(!p)
    return;

  switch(a.type)
  {
    case Action::Send:
      p->push_value(a.value.valid() ? a.value : ossia::value{ossia::impulse{}});
      break;

    case Action::Toggle: {
      const bool on = ossia::convert<bool>(p->value());
      if(p->get_value_type() == ossia::val_type::BOOL)
        p->push_value(!on);
      else
        p->push_value(on ? 0 : 1);
      break;
    }

    case Action::Step: {
      double v = ossia::convert<double>(p->value()) + a.step;
      const auto& dom = p->get_domain();
      if(auto min = ossia::get_min(dom); min.valid())
        v = std::max(v, ossia::convert<double>(min));
      if(auto max = ossia::get_max(dom); max.valid())
        v = std::min(v, ossia::convert<double>(max));
      if(p->get_value_type() == ossia::val_type::INT)
        p->push_value(int(std::lround(v)));
      else
        p->push_value(float(v));
      break;
    }

    default:
      break;
  }
}

bool Engine::evaluate(int page, int face, const Feedback& f)
{
  if(f.condition == Feedback::Script)
  {
    QString err;
    const bool res = m_scripts->condition(f.script, face, err);
    setError(page, face, 1, err);
    return res;
  }

  auto p = f.deckInput ? (deckParameter ? deckParameter(f.deckAddress()) : nullptr)
                       : resolve(f.address);
  if(!p)
    return false;
  const auto v = p->value();

  switch(f.condition)
  {
    case Feedback::IsTrue:
      return v.valid() && v.get_type() != ossia::val_type::IMPULSE
             && ossia::convert<bool>(v);
    case Feedback::IsFalse:
      return !(v.valid() && v.get_type() != ossia::val_type::IMPULSE
               && ossia::convert<bool>(v));
    default:
      break;
  }

  if(isNumeric(v) && (isNumeric(f.value) || f.value.get_type() == ossia::val_type::NONE))
  {
    const double a = ossia::convert<double>(v);
    const double b = ossia::convert<double>(f.value);
    switch(f.condition)
    {
      case Feedback::Equal:
        return std::abs(a - b) < 1e-6;
      case Feedback::NotEqual:
        return std::abs(a - b) >= 1e-6;
      case Feedback::Greater:
        return a > b;
      case Feedback::Less:
        return a < b;
      default:
        return false;
    }
  }

  const auto a = displayValue(v);
  const auto b = displayValue(f.value);
  switch(f.condition)
  {
    case Feedback::Equal:
      return a == b;
    case Feedback::NotEqual:
      return a != b;
    case Feedback::Greater:
      return a > b;
    case Feedback::Less:
      return a < b;
    default:
      return false;
  }
}

void Engine::setError(int page, int face, int slot, const QString& e)
{
  const auto k = std::make_tuple(page, face, slot);
  if(e.isEmpty())
    m_errors.erase(k);
  else
    m_errors[k] = e;
}

QString Engine::scriptError(int page, int face) const
{
  QString res;
  for(int slot = 0; slot < 3; slot++)
    if(auto it = m_errors.find({page, face, slot}); it != m_errors.end())
      res += (res.isEmpty() ? QString{} : QStringLiteral("\n")) + it->second;
  return res;
}

Style Engine::preview(int page, int face)
{
  const int keys = m_model.keyCount();
  if(page < 0 || page >= int(m_layout.pages.size()) || face < 0
     || face >= keys + m_model.encoders)
    return {};
  if(page == m_page && !m_remote && face < int(m_displayed.size()))
    return m_displayed[face];
  return face < keys ? computeFace(page, face) : computeDial(page, face - keys);
}

Style Engine::computeFace(int page, int key)
{
  const auto& b = m_layout.pages[page].buttons[key];
  Style s = b.style;
  if(auto it = m_overrides.find({page, key}); it != m_overrides.end())
    it->second.applyTo(s);

  bool active = false;
  if(b.mode == Button::Toggle)
  {
    auto it = m_toggles.find({page, key});
    active = it != m_toggles.end() && it->second;
  }
  else
  {
    active = page == m_page && m_keys[key].down && m_keys[key].page == m_page;
  }
  if(!active && deckParameter)
  {
    for(const auto& a : b.press)
      if(a.type == Action::Trigger && a.toggle)
        if(auto p = deckParameter(a.deckAddress()); p && ossia::convert<bool>(p->value()))
          active = true;
  }
  if(active)
    b.active.applyTo(s);

  for(const auto& f : b.feedbacks)
    if(evaluate(page, key, f))
      f.style.applyTo(s);

  QString err;
  s.text = m_scripts->text(s.text, key, err);
  setError(page, key, 0, err);
  return s;
}

Style Engine::computeDial(int page, int dial)
{
  const auto& d = m_layout.pages[page].dials[dial];
  const int face = m_model.keyCount() + dial;
  Style s = d.style;
  if(s.text.isEmpty() && !d.address.isEmpty())
  {
    const auto addr = State::parseAddress(d.address);
    s.text = addr && !addr->path.isEmpty() ? addr->path.back() : d.address;
    if(auto p = resolve(d.address))
      s.text += QStringLiteral("\n") + displayValue(p->value());
    setError(page, face, 0, {});
  }
  else
  {
    QString err;
    s.text = m_scripts->text(s.text, face, err);
    setError(page, face, 0, err);
  }
  return s;
}

void Engine::refresh()
{
  if(m_remote || m_shutdown)
    return;
  if(m_refreshing)
  {
    m_refreshAgain = true;
    return;
  }
  m_refreshing = true;
  int passes = 0;
  do
  {
    m_refreshAgain = false;
    const int keys = m_model.keyCount();
    for(int i = 0; i < int(m_displayed.size()); i++)
      display(i, i < keys ? computeFace(m_page, i) : computeDial(m_page, i - keys));
  } while(m_refreshAgain && ++passes < 3);
  m_refreshing = false;
}
}
