#pragma once
#include <Gfx/GfxParameter.hpp>

#include <StreamDeck/Deck.hpp>

#include <ossia/detail/hash_map.hpp>
#include <ossia/network/base/device.hpp>

#include <QPointer>

#include <memory>
#include <mutex>

namespace StreamDeck
{
class Engine;

class deck_protocol final : public Gfx::gfx_protocol_base
{
public:
  deck_protocol(Gfx::GfxExecutionAction& ctx, Engine& engine);
  ~deck_protocol() override;

  void set_device(ossia::net::device_base& dev) override;
  bool push(const ossia::net::parameter_base&, const ossia::value& v) override;

  //! Main thread. Rebuilds /pages when the number of pages changed.
  bool updatePages(int count);
  void setInfo(const std::string& serial, const std::string& firmware);

  //! Main thread. Creates the addresses of the layout's triggers and inputs,
  //! removes those it no longer has. True if the tree changed.
  bool updateDeckAddresses(const std::vector<std::pair<std::string, ossia::val_type>>& addrs);
  ossia::net::parameter_base* deckParameter(const std::string& path) const;
  //! Sets a value of the tree and tells score about it.
  void write(ossia::net::parameter_base* p, const ossia::value& v) { notify(p, v); }

  void notifyKey(int key, bool pressed);
  void notifyButton(int page, int key, bool pressed);
  void notifyPage(int page);
  void notifyBrightness(int b);
  void notifyTouchKey(int key, bool pressed);
  void notifyEncoderPress(int encoder, bool pressed);
  void notifyEncoderTurn(int encoder, int delta);
  void notifyTouch(InputEvent::Kind kind, int x, int y, int x2, int y2);

private:
  enum class Field : uint8_t
  {
    Page,
    Brightness,
    KeyPressed,
    Color,
    Text,
    TextColor,
    Image,
    EncoderPosition,
    DeckAddress,
  };
  struct Binding
  {
    Field field{};
    int page{};
    int index{};
  };

  void notify(ossia::net::parameter_base* p, const ossia::value& v);

  QPointer<Engine> m_engine;
  const Model& m_model;
  ossia::net::device_base* m_device{};

  std::mutex m_mutex;
  ossia::hash_map<const ossia::net::parameter_base*, Binding> m_bindings;

  ossia::net::parameter_base* m_page{};
  ossia::net::parameter_base* m_brightness{};
  ossia::net::parameter_base* m_serial{};
  ossia::net::parameter_base* m_firmware{};
  std::vector<ossia::net::parameter_base*> m_keys;
  std::vector<std::vector<ossia::net::parameter_base*>> m_buttons;
  std::vector<ossia::net::parameter_base*> m_touchKeys;
  std::vector<ossia::net::parameter_base*> m_encoderPressed;
  std::vector<ossia::net::parameter_base*> m_encoderTurn;
  std::vector<ossia::net::parameter_base*> m_encoderPosition;
  std::vector<int> m_positions;
  std::vector<std::pair<std::string, ossia::net::parameter_base*>> m_deckAddresses;
  ossia::net::parameter_base* m_tap{};
  ossia::net::parameter_base* m_longPress{};
  ossia::net::parameter_base* m_swipe{};
};

class deck_root_node final : public ossia::net::node_base
{
public:
  deck_root_node(
      ossia::net::device_base& dev, Gfx::gfx_protocol_base& proto,
      score::gfx::Node* node, std::string name);
  ~deck_root_node() override;

  Gfx::gfx_parameter_base* get_parameter() const override { return m_parameter.get(); }

private:
  ossia::net::device_base& get_device() const override { return m_device; }
  ossia::net::node_base* get_parent() const override { return nullptr; }
  ossia::net::node_base& set_name(std::string) override { return *this; }
  ossia::net::parameter_base* create_parameter(ossia::val_type) override
  {
    return m_parameter.get();
  }
  bool remove_parameter() override { return false; }
  std::unique_ptr<ossia::net::node_base> make_child(const std::string& name) override;
  void removing_child(ossia::net::node_base&) override { }

  ossia::net::device_base& m_device;
  std::unique_ptr<Gfx::gfx_parameter_base> m_parameter;
};

class deck_device final : public ossia::net::device_base
{
public:
  deck_device(
      std::unique_ptr<deck_protocol> proto, score::gfx::Node* node, std::string name);
  ~deck_device() override;

  deck_protocol& protocol() const noexcept
  {
    return static_cast<deck_protocol&>(*m_protocol);
  }
  const deck_root_node& get_root_node() const override { return m_root; }
  deck_root_node& get_root_node() override { return m_root; }

private:
  deck_root_node m_root;
};
}
