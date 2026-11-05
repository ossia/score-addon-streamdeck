#pragma once
#include <ossia/network/value/value.hpp>

#include <QColor>
#include <QJsonObject>
#include <QString>

#include <optional>
#include <vector>

namespace StreamDeck
{
enum class Align : uint8_t
{
  Center,
  Top,
  Bottom
};

struct Style
{
  QColor background{Qt::black};
  QColor textColor{Qt::white};
  //! A JavaScript template literal when it contains ${...}.
  QString text;
  QString image;
  //! Pixels; 0 fits the text to the key.
  int textSize{};
  Align align{Align::Center};

  bool operator==(const Style&) const = default;
};

//! The parts of a style that a state (active, feedback) replaces.
struct StyleOverride
{
  std::optional<QColor> background;
  std::optional<QColor> textColor;
  std::optional<QString> text;
  std::optional<QString> image;

  bool empty() const noexcept { return !background && !textColor && !text && !image; }
  void applyTo(Style& s) const;
  bool operator==(const StyleOverride&) const = default;
};

struct Action
{
  enum Type : uint8_t
  {
    Send,
    Toggle,
    Step,
    GotoPage,
    NextPage,
    PreviousPage,
    Script,
    Trigger,
  };

  Type type{Send};
  QString address;
  ossia::value value;
  double step{1.};
  int page{};
  QString script;
  bool toggle{};
  //! Milliseconds after the key event.
  int delay{};

  bool needsAddress() const noexcept { return type == Send || type == Toggle || type == Step; }
  QString deckAddress() const { return QStringLiteral("actions/") + address; }
  bool operator==(const Action&) const = default;
};

struct Feedback
{
  enum Condition : uint8_t
  {
    IsTrue,
    IsFalse,
    Equal,
    NotEqual,
    Greater,
    Less,
    Script
  };

  QString address;
  Condition condition{IsTrue};
  ossia::value value;
  QString script;
  bool deckInput{};

  QString deckAddress() const { return QStringLiteral("feedbacks/") + address; }
  StyleOverride style;

  bool needsAddress() const noexcept { return condition != Script; }
  bool needsValue() const noexcept { return condition >= Equal && condition != Script; }
  bool operator==(const Feedback&) const = default;
};

struct Button
{
  enum Mode : uint8_t
  {
    Momentary,
    Toggle
  };

  Style style;
  Mode mode{Momentary};
  //! The key must be held this long (ms) before its actions run.
  int hold{};
  std::vector<Action> press;
  std::vector<Action> release;
  //! Applied while pressed (momentary) or while on (toggle).
  StyleOverride active;
  std::vector<Feedback> feedbacks;

  bool empty() const noexcept { return *this == Button{}; }
  bool operator==(const Button&) const = default;
};

struct Dial
{
  QString address;
  double step{0.01};
  std::vector<Action> press;
  Style style;

  bool empty() const noexcept { return *this == Dial{}; }
  bool operator==(const Dial&) const = default;
};

struct Page
{
  QString name;
  bool texture{};
  std::vector<Button> buttons;
  std::vector<Dial> dials;

  bool operator==(const Page&) const = default;
};

struct Layout
{
  std::vector<Page> pages;

  //! At least one page, and every page sized for the model.
  void normalize(int keys, int dials);

  //! The addresses the layout creates in the deck's tree, relative to it.
  std::vector<std::pair<QString, ossia::val_type>> deckAddresses() const;
  bool operator==(const Layout&) const = default;
};

QJsonObject toJson(const Layout& l);
Layout layoutFromJson(const QJsonObject& obj);

//! A path under the deck's root: valid names, separated by slashes.
QString deckPath(const QString& s);

QString valueToString(const ossia::value& v);
ossia::value valueFromString(const QString& s);
}
