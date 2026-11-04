#include "Protocol.hpp"

#include <ossia/detail/fmt.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/dataspace/dataspace.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/generic/generic_node.hpp>

#include <QColor>

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

ossia::net::parameter_base*
make(ossia::net::node_base& parent, std::string_view path, ossia::val_type t,
     ossia::access_mode access)
{
  auto& node = ossia::net::create_node(parent, path);
  auto p = node.create_parameter(t);
  p->set_access(access);
  return p;
}
}

deck_protocol::deck_protocol(
    Gfx::GfxExecutionAction& ctx, std::shared_ptr<Deck> deck, int brightness)
    : gfx_protocol_base{ctx}
    , m_deck{std::move(deck)}
    , m_brightness{brightness}
{
}

deck_protocol::~deck_protocol() = default;

void deck_protocol::createFace(ossia::net::node_base& node, int face)
{
  using ossia::access_mode;
  using ossia::val_type;

  auto color = make(node, "color", val_type::VEC4F, access_mode::BI);
  color->set_unit(ossia::rgba_u{});
  color->set_value(ossia::vec4f{0.f, 0.f, 0.f, 1.f});
  m_bindings[color] = {face, Field::Color};

  auto text = make(node, "text", val_type::STRING, access_mode::BI);
  m_bindings[text] = {face, Field::Text};

  auto textColor = make(node, "text_color", val_type::VEC4F, access_mode::BI);
  textColor->set_unit(ossia::rgba_u{});
  textColor->set_value(ossia::vec4f{1.f, 1.f, 1.f, 1.f});
  m_bindings[textColor] = {face, Field::TextColor};

  auto textSize = make(node, "text_size", val_type::INT, access_mode::BI);
  textSize->set_domain(ossia::make_domain(0, 256));
  m_bindings[textSize] = {face, Field::TextSize};

  auto image = make(node, "image", val_type::STRING, access_mode::BI);
  m_bindings[image] = {face, Field::Image};
}

void deck_protocol::set_device(ossia::net::device_base& dev)
{
  using ossia::access_mode;
  using ossia::val_type;

  m_device = &dev;
  auto& root = dev.get_root_node();
  const auto& model = m_deck->model();

  make(root, "info/model", val_type::STRING, access_mode::GET)
      ->set_value(std::string(model.name));
  make(root, "info/serial", val_type::STRING, access_mode::GET)->set_value(m_deck->serial());
  make(root, "info/firmware", val_type::STRING, access_mode::GET)
      ->set_value(m_deck->firmware());

  for(int i = 0; i < model.keyCount(); i++)
  {
    auto& key = ossia::net::create_node(root, fmt::format("key/{}", i + 1));
    m_keys.push_back(make(key, "pressed", val_type::BOOL, access_mode::GET));
    if(model.hasDisplay())
      createFace(key, i);
  }

  for(int i = 0; i < model.touchKeys; i++)
    m_touchKeys.push_back(make(
        root, fmt::format("touch_key/{}/pressed", i + 1), val_type::BOOL,
        access_mode::GET));

  m_positions.resize(model.encoders);
  for(int i = 0; i < model.encoders; i++)
  {
    auto& enc = ossia::net::create_node(root, fmt::format("encoder/{}", i + 1));
    m_encoderPressed.push_back(make(enc, "pressed", val_type::BOOL, access_mode::GET));
    m_encoderTurn.push_back(make(enc, "turn", val_type::INT, access_mode::GET));
    m_encoderPosition.push_back(make(enc, "position", val_type::INT, access_mode::BI));
  }

  if(model.hasLcd())
  {
    for(int i = 0; i < model.encoders; i++)
      createFace(
          ossia::net::create_node(root, fmt::format("lcd/{}", i + 1)),
          model.keyCount() + i);

    m_tap = make(root, "touch/tap", val_type::VEC2F, access_mode::GET);
    m_longPress = make(root, "touch/long_press", val_type::VEC2F, access_mode::GET);
    m_swipe = make(root, "touch/swipe", val_type::VEC4F, access_mode::GET);
  }

  if(model.hasDisplay())
  {
    auto brightness = make(root, "brightness", val_type::INT, access_mode::BI);
    brightness->set_domain(ossia::make_domain(0, 100));
    brightness->set_value(m_brightness);
    m_bindings[brightness] = {0, Field::Brightness};

    auto clear = make(root, "clear", val_type::IMPULSE, access_mode::SET);
    m_bindings[clear] = {0, Field::Clear};
  }
}

bool deck_protocol::push(const ossia::net::parameter_base& p, const ossia::value& v)
{
  for(std::size_t i = 0; i < m_encoderPosition.size(); i++)
  {
    if(&p == m_encoderPosition[i])
    {
      std::lock_guard lock{m_positionMutex};
      m_positions[i] = ossia::convert<int>(v);
      return true;
    }
  }

  auto it = m_bindings.find(&p);
  if(it == m_bindings.end())
    return false;

  const auto [face, field] = it->second;
  switch(field)
  {
    case Field::Brightness:
      m_deck->setBrightness(ossia::convert<int>(v));
      break;
    case Field::Clear:
      m_deck->clear();
      break;
    case Field::Color:
      m_deck->updateFace(face, [c = toColor(v)](Face& f) { f.background = c; });
      break;
    case Field::Text:
      m_deck->updateFace(face, [t = QString::fromStdString(ossia::convert<std::string>(v))](
                                   Face& f) { f.text = t; });
      break;
    case Field::TextColor:
      m_deck->updateFace(face, [c = toColor(v)](Face& f) { f.textColor = c; });
      break;
    case Field::TextSize:
      m_deck->updateFace(face, [s = ossia::convert<int>(v)](Face& f) { f.textSize = s; });
      break;
    case Field::Image:
      m_deck->updateFace(face, [t = QString::fromStdString(ossia::convert<std::string>(v))](
                                   Face& f) { f.imagePath = t; });
      break;
  }
  return true;
}

void deck_protocol::notify(ossia::net::parameter_base* p, const ossia::value& v)
{
  if(!p || !m_device)
    return;
  p->set_value(v);
  m_device->on_message(*p);
}

void deck_protocol::onKey(int key, bool pressed)
{
  if(key >= 0 && key < int(m_keys.size()))
    notify(m_keys[key], pressed);
}

void deck_protocol::onTouchKey(int key, bool pressed)
{
  if(key >= 0 && key < int(m_touchKeys.size()))
    notify(m_touchKeys[key], pressed);
}

void deck_protocol::onEncoderPress(int encoder, bool pressed)
{
  if(encoder >= 0 && encoder < int(m_encoderPressed.size()))
    notify(m_encoderPressed[encoder], pressed);
}

void deck_protocol::onEncoderTurn(int encoder, int delta)
{
  if(encoder < 0 || encoder >= int(m_encoderTurn.size()))
    return;
  notify(m_encoderTurn[encoder], delta);
  int position{};
  {
    std::lock_guard lock{m_positionMutex};
    position = (m_positions[encoder] += delta);
  }
  notify(m_encoderPosition[encoder], position);
}

void deck_protocol::onTouch(InputEvent::Kind kind, int x, int y, int x2, int y2)
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
