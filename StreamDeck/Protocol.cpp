#include "Protocol.hpp"

#include <ossia/detail/fmt.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/dataspace/dataspace.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/generic/generic_node.hpp>

#include <StreamDeck/Engine.hpp>

namespace StreamDeck
{
namespace
{
QColor toColor(const ossia::value& v)
{
  const auto c = ossia::convert<ossia::vec4f>(v);
  return QColor::fromRgbF(
      std::clamp(c[0], 0.f, 1.f), std::clamp(c[1], 0.f, 1.f), std::clamp(c[2], 0.f, 1.f),
      std::clamp(c[3], 0.f, 1.f));
}

ossia::net::parameter_base* make(
    ossia::net::node_base& parent, std::string_view path, ossia::val_type t,
    ossia::access_mode access)
{
  auto& node = ossia::net::create_node(parent, path);
  auto p = node.create_parameter(t);
  p->set_access(access);
  return p;
}
}

deck_protocol::deck_protocol(Gfx::GfxExecutionAction& ctx, Engine& engine)
    : gfx_protocol_base{ctx}
    , m_engine{&engine}
    , m_model{engine.model()}
{
}

deck_protocol::~deck_protocol() = default;

void deck_protocol::set_device(ossia::net::device_base& dev)
{
  using ossia::access_mode;
  using ossia::val_type;

  m_device = &dev;
  auto& root = dev.get_root_node();
  std::lock_guard lock{m_mutex};

  make(root, "info/model", val_type::STRING, access_mode::GET)
      ->set_value(std::string(m_model.name));
  m_serial = make(root, "info/serial", val_type::STRING, access_mode::GET);
  m_firmware = make(root, "info/firmware", val_type::STRING, access_mode::GET);

  m_page = make(root, "page", val_type::INT, access_mode::BI);
  m_page->set_value(m_engine->page() + 1);
  m_bindings[m_page] = {Field::Page};

  for(int i = 0; i < m_model.keyCount(); i++)
  {
    auto p = make(
        root, fmt::format("key/{}/pressed", i + 1), val_type::BOOL, access_mode::BI);
    m_keys.push_back(p);
    m_bindings[p] = {Field::KeyPressed, 0, i};
  }

  for(int i = 0; i < m_model.touchKeys; i++)
    m_touchKeys.push_back(make(
        root, fmt::format("touch_key/{}/pressed", i + 1), val_type::BOOL,
        access_mode::GET));

  m_positions.resize(m_model.encoders);
  for(int i = 0; i < m_model.encoders; i++)
  {
    auto& enc = ossia::net::create_node(root, fmt::format("encoder/{}", i + 1));
    m_encoderPressed.push_back(make(enc, "pressed", val_type::BOOL, access_mode::GET));
    m_encoderTurn.push_back(make(enc, "turn", val_type::INT, access_mode::GET));
    auto pos = make(enc, "position", val_type::INT, access_mode::BI);
    m_encoderPosition.push_back(pos);
    m_bindings[pos] = {Field::EncoderPosition, 0, i};
  }

  if(m_model.hasLcd())
  {
    m_tap = make(root, "touch/tap", val_type::VEC2F, access_mode::GET);
    m_longPress = make(root, "touch/long_press", val_type::VEC2F, access_mode::GET);
    m_swipe = make(root, "touch/swipe", val_type::VEC4F, access_mode::GET);
  }

  if(m_model.hasDisplay())
  {
    m_brightness = make(root, "brightness", val_type::INT, access_mode::BI);
    m_brightness->set_domain(ossia::make_domain(0, 100));
    m_brightness->set_value(m_engine->brightness());
    m_bindings[m_brightness] = {Field::Brightness};
  }
}

bool deck_protocol::updatePages(int count)
{
  using ossia::access_mode;
  using ossia::val_type;

  if(!m_device || int(m_buttons.size()) == count)
    return false;

  auto& root = m_device->get_root_node();
  std::lock_guard lock{m_mutex};
  for(auto& page : m_buttons)
    for(auto p : page)
      m_bindings.erase(p);
  m_buttons.clear();

  if(auto pages = ossia::net::find_node(root, "/pages"))
    root.remove_child(*pages);

  m_page->set_domain(ossia::make_domain(1, std::max(1, count)));

  for(int pg = 0; pg < count; pg++)
  {
    auto& buttons = m_buttons.emplace_back();
    for(int k = 0; k < m_model.keyCount(); k++)
    {
      auto& key
          = ossia::net::create_node(root, fmt::format("pages/{}/key/{}", pg + 1, k + 1));
      buttons.push_back(make(key, "pressed", val_type::BOOL, access_mode::GET));
      if(!m_model.hasDisplay())
        continue;

      auto color = make(key, "color", val_type::VEC4F, access_mode::SET);
      color->set_unit(ossia::rgba_u{});
      m_bindings[color] = {Field::Color, pg, k};
      auto text = make(key, "text", val_type::STRING, access_mode::SET);
      m_bindings[text] = {Field::Text, pg, k};
      auto textColor = make(key, "text_color", val_type::VEC4F, access_mode::SET);
      textColor->set_unit(ossia::rgba_u{});
      m_bindings[textColor] = {Field::TextColor, pg, k};
      auto image = make(key, "image", val_type::STRING, access_mode::SET);
      m_bindings[image] = {Field::Image, pg, k};
    }
  }
  return true;
}

void deck_protocol::setInfo(const std::string& serial, const std::string& firmware)
{
  notify(m_serial, serial);
  notify(m_firmware, firmware);
}

bool deck_protocol::push(const ossia::net::parameter_base& p, const ossia::value& v)
{
  Binding b;
  {
    std::lock_guard lock{m_mutex};
    auto it = m_bindings.find(&p);
    if(it == m_bindings.end())
      return false;
    b = it->second;
    if(b.field == Field::EncoderPosition)
    {
      m_positions[b.index] = ossia::convert<int>(v);
      return true;
    }
  }

  auto engine = m_engine.data();
  if(!engine)
    return false;

  QMetaObject::invokeMethod(engine, [engine, b, v] {
    switch(b.field)
    {
      case Field::Page:
        engine->setPage(ossia::convert<int>(v) - 1);
        break;
      case Field::Brightness:
        engine->setBrightness(ossia::convert<int>(v));
        break;
      case Field::KeyPressed:
        engine->press(b.index, ossia::convert<bool>(v));
        break;
      case Field::Color:
        engine->setOverride(b.page, b.index, {.background = toColor(v)});
        break;
      case Field::TextColor:
        engine->setOverride(b.page, b.index, {.textColor = toColor(v)});
        break;
      case Field::Text:
        engine->setOverride(
            b.page, b.index, {.text = QString::fromStdString(ossia::convert<std::string>(v))});
        break;
      case Field::Image:
        engine->setOverride(
            b.page, b.index,
            {.image = QString::fromStdString(ossia::convert<std::string>(v))});
        break;
      case Field::EncoderPosition:
        break;
    }
  }, Qt::QueuedConnection);
  return true;
}

void deck_protocol::notify(ossia::net::parameter_base* p, const ossia::value& v)
{
  if(!p || !m_device)
    return;
  p->set_value(v);
  m_device->on_message(*p);
}

void deck_protocol::notifyKey(int key, bool pressed)
{
  if(key >= 0 && key < int(m_keys.size()))
    notify(m_keys[key], pressed);
}

void deck_protocol::notifyButton(int page, int key, bool pressed)
{
  ossia::net::parameter_base* p{};
  {
    std::lock_guard lock{m_mutex};
    if(page >= 0 && page < int(m_buttons.size()) && key >= 0
       && key < int(m_buttons[page].size()))
      p = m_buttons[page][key];
  }
  notify(p, pressed);
}

void deck_protocol::notifyPage(int page)
{
  notify(m_page, page + 1);
}

void deck_protocol::notifyBrightness(int b)
{
  notify(m_brightness, b);
}

void deck_protocol::notifyTouchKey(int key, bool pressed)
{
  if(key >= 0 && key < int(m_touchKeys.size()))
    notify(m_touchKeys[key], pressed);
}

void deck_protocol::notifyEncoderPress(int encoder, bool pressed)
{
  if(encoder >= 0 && encoder < int(m_encoderPressed.size()))
    notify(m_encoderPressed[encoder], pressed);
}

void deck_protocol::notifyEncoderTurn(int encoder, int delta)
{
  if(encoder < 0 || encoder >= int(m_encoderTurn.size()))
    return;
  notify(m_encoderTurn[encoder], delta);
  int position{};
  {
    std::lock_guard lock{m_mutex};
    position = (m_positions[encoder] += delta);
  }
  notify(m_encoderPosition[encoder], position);
}

void deck_protocol::notifyTouch(InputEvent::Kind kind, int x, int y, int x2, int y2)
{
  switch(kind)
  {
    case InputEvent::TouchShort:
      notify(m_tap, ossia::vec2f{float(x), float(y)});
      break;
    case InputEvent::TouchLong:
      notify(m_longPress, ossia::vec2f{float(x), float(y)});
      break;
    case InputEvent::TouchSwipe:
      notify(m_swipe, ossia::vec4f{float(x), float(y), float(x2), float(y2)});
      break;
    default:
      break;
  }
}

deck_root_node::deck_root_node(
    ossia::net::device_base& dev, Gfx::gfx_protocol_base& proto, score::gfx::Node* node,
    std::string name)
    : m_device{dev}
    , m_parameter{std::make_unique<Gfx::gfx_parameter_base>(*this, node, proto.context)}
{
  m_name = std::move(name);
}

deck_root_node::~deck_root_node()
{
  clear_children();
  m_parameter.reset();
}

std::unique_ptr<ossia::net::node_base>
deck_root_node::make_child(const std::string& name)
{
  return std::make_unique<ossia::net::generic_node>(name, m_device, *this);
}

deck_device::deck_device(
    std::unique_ptr<deck_protocol> proto, score::gfx::Node* node, std::string name)
    : ossia::net::device_base{std::move(proto)}
    , m_root{*this, *static_cast<deck_protocol*>(m_protocol.get()), node, name}
{
  m_protocol->set_device(*this);
}

deck_device::~deck_device()
{
  m_root.clear_children();
}
}
