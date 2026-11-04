#pragma once
#include <StreamDeck/Layout.hpp>
#include <StreamDeck/Model.hpp>

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include <verdigris>

namespace score
{
struct DocumentContext;
}
namespace ossia::net
{
class parameter_base;
}

namespace StreamDeck
{
class Deck;
class RemoteControl;
class Scripts;

//! The parameter at "device:/path" in the document, if it exists now.
ossia::net::parameter_base*
resolveAddress(const score::DocumentContext& ctx, const QString& address);

/**
 * Runs a layout: which page is shown, what each key looks like, what a press
 * does. Lives on the main thread, and runs whether or not a deck is plugged:
 * the editor's emulator presses and shows the same keys.
 */
class Engine final : public QObject
{
  W_OBJECT(Engine)
public:
  Engine(const score::DocumentContext& ctx, const Model& model);
  ~Engine() override;

  const Model& model() const noexcept { return m_model; }
  const Layout& layout() const noexcept { return m_layout; }
  void setLayout(Layout l);

  int page() const noexcept { return m_page; }
  void setPage(int page);
  void nextPage();
  void previousPage();

  int brightness() const noexcept { return m_brightness; }
  void setBrightness(int b);

  int faceCount() const noexcept { return int(m_displayed.size()); }
  const Style& face(int i) const { return m_displayed[i]; }
  bool isPressed(int key) const;
  bool isTexture(int face) const;

  //! What a face of any page looks like now, as the deck would show it.
  Style preview(int page, int face);
  //! The last error of the scripts of a face of a page, if any.
  QString scriptError(int page, int face) const;

  void press(int key, bool down);
  void pressDial(int dial, bool down);
  void turnDial(int dial, int delta);

  //! A field of a key's style, set at run time from the device tree.
  void setOverride(int page, int key, const StyleOverride& o);

  //! Stops running: nothing happens any more until it is deleted.
  void shutdown();

  void setDeck(std::shared_ptr<Deck> deck);
  //! While set, the remote control draws the keys and gets the presses.
  void setRemoteControl(RemoteControl* rc);
  void display(int face, const Style& s);

  std::function<void(int key, bool pressed)> onKey;
  std::function<void(int page, int key, bool pressed)> onButton;
  std::function<void(int page)> onPage;
  //! The addresses of the deck itself that the layout made, by path.
  std::function<ossia::net::parameter_base*(const QString&)> deckParameter;
  std::function<void(ossia::net::parameter_base*, const ossia::value&)> writeDeck;

  void faceChanged(int face) W_SIGNAL(faceChanged, face);
  void pageChanged(int page) W_SIGNAL(pageChanged, page);
  void layoutChanged() W_SIGNAL(layoutChanged);

private:
  struct KeyState
  {
    bool down{};
    bool fired{};
    int page{};
    int generation{};
  };

  void refresh();
  void fire(int page, int key, bool down);
  Style computeFace(int page, int key);
  Style computeDial(int page, int dial);
  void run(int page, int face, std::vector<Action> actions);
  void runNow(int page, int face, const Action& a);
  ossia::net::parameter_base* resolve(const QString& address) const;
  bool evaluate(int page, int face, const Feedback& f);
  void setError(int page, int face, int slot, const QString& e);
  void applyTextureMask();

  const score::DocumentContext& m_ctx;
  const Model& m_model;
  Layout m_layout;
  int m_page{};
  int m_brightness{70};

  std::vector<Style> m_displayed;
  std::vector<KeyState> m_keys;
  std::map<std::pair<int, int>, bool> m_toggles;
  std::map<std::pair<int, int>, StyleOverride> m_overrides;
  std::map<std::tuple<int, int, int>, QString> m_errors;
  std::unique_ptr<Scripts> m_scripts;
  bool m_refreshing{};
  bool m_shutdown{};
  bool m_refreshAgain{};

  std::shared_ptr<Deck> m_deck;
  QPointer<RemoteControl> m_remote;
  QTimer m_timer;
};
}
