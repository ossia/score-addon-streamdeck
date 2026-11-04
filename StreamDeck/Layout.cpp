#include "Layout.hpp"

#include <State/Value.hpp>
#include <State/ValueConversion.hpp>

#include <QJsonArray>

namespace StreamDeck
{
void StyleOverride::applyTo(Style& s) const
{
  if(background)
    s.background = *background;
  if(textColor)
    s.textColor = *textColor;
  if(text)
    s.text = *text;
  if(image)
    s.image = *image;
}

void Layout::normalize(int keys, int dials)
{
  if(pages.empty())
    pages.push_back({.name = QStringLiteral("Page 1")});
  for(auto& p : pages)
  {
    if(int(p.buttons.size()) < keys)
      p.buttons.resize(keys);
    if(int(p.dials.size()) < dials)
      p.dials.resize(dials);
  }
}

QString valueToString(const ossia::value& v)
{
  if(!v.valid())
    return {};
  return State::convert::toPrettyString(v);
}

ossia::value valueFromString(const QString& s)
{
  const auto str = s.trimmed();
  if(str.isEmpty())
    return {};
  if(auto v = State::parseValue(str.toStdString()))
    return *v;
  return str.toStdString();
}

namespace
{
QString colorToString(const QColor& c)
{
  return c.name(QColor::HexArgb);
}

QColor colorFromJson(const QJsonValue& v, QColor def)
{
  if(!v.isString())
    return def;
  QColor c = QColor::fromString(v.toString());
  return c.isValid() ? c : def;
}

const char* alignName(Align a)
{
  switch(a)
  {
    case Align::Top:
      return "top";
    case Align::Bottom:
      return "bottom";
    default:
      return "center";
  }
}

Align alignFromString(const QString& s)
{
  if(s == "top")
    return Align::Top;
  if(s == "bottom")
    return Align::Bottom;
  return Align::Center;
}

QJsonObject toJson(const Style& s)
{
  const Style def;
  QJsonObject o;
  if(s.background != def.background)
    o["background"] = colorToString(s.background);
  if(s.textColor != def.textColor)
    o["textColor"] = colorToString(s.textColor);
  if(!s.text.isEmpty())
    o["text"] = s.text;
  if(!s.image.isEmpty())
    o["image"] = s.image;
  if(s.textSize != 0)
    o["textSize"] = s.textSize;
  if(s.align != Align::Center)
    o["align"] = alignName(s.align);
  return o;
}

Style styleFromJson(const QJsonObject& o)
{
  Style s;
  s.background = colorFromJson(o["background"], s.background);
  s.textColor = colorFromJson(o["textColor"], s.textColor);
  s.text = o["text"].toString();
  s.image = o["image"].toString();
  s.textSize = o["textSize"].toInt();
  s.align = alignFromString(o["align"].toString());
  return s;
}

QJsonObject toJson(const StyleOverride& s)
{
  QJsonObject o;
  if(s.background)
    o["background"] = colorToString(*s.background);
  if(s.textColor)
    o["textColor"] = colorToString(*s.textColor);
  if(s.text)
    o["text"] = *s.text;
  if(s.image)
    o["image"] = *s.image;
  return o;
}

StyleOverride overrideFromJson(const QJsonObject& o)
{
  StyleOverride s;
  if(o.contains("background"))
    s.background = colorFromJson(o["background"], Qt::black);
  if(o.contains("textColor"))
    s.textColor = colorFromJson(o["textColor"], Qt::white);
  if(o.contains("text"))
    s.text = o["text"].toString();
  if(o.contains("image"))
    s.image = o["image"].toString();
  return s;
}

constexpr const char* action_names[]
    = {"send", "toggle", "step", "page", "next_page", "previous_page"};
constexpr const char* condition_names[]
    = {"true", "false", "equal", "not_equal", "greater", "less"};

template <std::size_t N>
int indexOf(const char* const (&names)[N], const QString& s)
{
  for(std::size_t i = 0; i < N; i++)
    if(s == QLatin1String(names[i]))
      return int(i);
  return 0;
}

QJsonArray toJson(const std::vector<Action>& actions)
{
  QJsonArray arr;
  for(const auto& a : actions)
  {
    QJsonObject o;
    o["type"] = action_names[a.type];
    if(a.needsAddress())
      o["address"] = a.address;
    if(a.type == Action::Send && a.value.valid())
      o["value"] = valueToString(a.value);
    if(a.type == Action::Step)
      o["step"] = a.step;
    if(a.type == Action::GotoPage)
      o["page"] = a.page;
    if(a.delay > 0)
      o["delay"] = a.delay;
    arr.append(o);
  }
  return arr;
}

std::vector<Action> actionsFromJson(const QJsonArray& arr)
{
  std::vector<Action> res;
  for(const auto& v : arr)
  {
    const auto o = v.toObject();
    Action a;
    a.type = Action::Type(indexOf(action_names, o["type"].toString()));
    a.address = o["address"].toString();
    a.value = valueFromString(o["value"].toString());
    a.step = o["step"].toDouble(1.);
    a.page = o["page"].toInt();
    a.delay = o["delay"].toInt();
    res.push_back(std::move(a));
  }
  return res;
}

QJsonObject toJson(const Button& b)
{
  QJsonObject o;
  if(auto s = toJson(b.style); !s.isEmpty())
    o["style"] = s;
  if(b.mode == Button::Toggle)
    o["mode"] = "toggle";
  if(b.hold > 0)
    o["hold"] = b.hold;
  if(!b.press.empty())
    o["press"] = toJson(b.press);
  if(!b.release.empty())
    o["release"] = toJson(b.release);
  if(!b.active.empty())
    o["active"] = toJson(b.active);
  if(!b.feedbacks.empty())
  {
    QJsonArray arr;
    for(const auto& f : b.feedbacks)
    {
      QJsonObject fo;
      fo["address"] = f.address;
      fo["condition"] = condition_names[f.condition];
      if(f.needsValue())
        fo["value"] = valueToString(f.value);
      fo["style"] = toJson(f.style);
      arr.append(fo);
    }
    o["feedbacks"] = arr;
  }
  return o;
}

Button buttonFromJson(const QJsonObject& o)
{
  Button b;
  b.style = styleFromJson(o["style"].toObject());
  b.mode = o["mode"].toString() == "toggle" ? Button::Toggle : Button::Momentary;
  b.hold = o["hold"].toInt();
  b.press = actionsFromJson(o["press"].toArray());
  b.release = actionsFromJson(o["release"].toArray());
  b.active = overrideFromJson(o["active"].toObject());
  for(const auto& v : o["feedbacks"].toArray())
  {
    const auto fo = v.toObject();
    Feedback f;
    f.address = fo["address"].toString();
    f.condition = Feedback::Condition(indexOf(condition_names, fo["condition"].toString()));
    f.value = valueFromString(fo["value"].toString());
    f.style = overrideFromJson(fo["style"].toObject());
    b.feedbacks.push_back(std::move(f));
  }
  return b;
}

QJsonObject toJson(const Dial& d)
{
  QJsonObject o;
  if(!d.address.isEmpty())
    o["address"] = d.address;
  o["step"] = d.step;
  if(!d.press.empty())
    o["press"] = toJson(d.press);
  if(auto s = toJson(d.style); !s.isEmpty())
    o["style"] = s;
  return o;
}

Dial dialFromJson(const QJsonObject& o)
{
  Dial d;
  d.address = o["address"].toString();
  d.step = o["step"].toDouble(0.01);
  d.press = actionsFromJson(o["press"].toArray());
  d.style = styleFromJson(o["style"].toObject());
  return d;
}
}

QJsonObject toJson(const Layout& l)
{
  QJsonArray pages;
  for(const auto& p : l.pages)
  {
    QJsonObject po;
    po["name"] = p.name;
    if(p.texture)
      po["texture"] = true;

    QJsonObject buttons;
    for(std::size_t i = 0; i < p.buttons.size(); i++)
      if(!p.buttons[i].empty())
        buttons[QString::number(i + 1)] = toJson(p.buttons[i]);
    if(!buttons.isEmpty())
      po["buttons"] = buttons;

    QJsonObject dials;
    for(std::size_t i = 0; i < p.dials.size(); i++)
      if(!p.dials[i].empty())
        dials[QString::number(i + 1)] = toJson(p.dials[i]);
    if(!dials.isEmpty())
      po["dials"] = dials;

    pages.append(po);
  }
  QJsonObject o;
  o["pages"] = pages;
  return o;
}

Layout layoutFromJson(const QJsonObject& obj)
{
  Layout l;
  for(const auto& v : obj["pages"].toArray())
  {
    const auto po = v.toObject();
    Page p;
    p.name = po["name"].toString();
    p.texture = po["texture"].toBool();

    const auto buttons = po["buttons"].toObject();
    for(auto it = buttons.begin(); it != buttons.end(); ++it)
    {
      const int idx = it.key().toInt() - 1;
      if(idx < 0 || idx >= 256)
        continue;
      if(int(p.buttons.size()) <= idx)
        p.buttons.resize(idx + 1);
      p.buttons[idx] = buttonFromJson(it.value().toObject());
    }

    const auto dials = po["dials"].toObject();
    for(auto it = dials.begin(); it != dials.end(); ++it)
    {
      const int idx = it.key().toInt() - 1;
      if(idx < 0 || idx >= 16)
        continue;
      if(int(p.dials.size()) <= idx)
        p.dials.resize(idx + 1);
      p.dials[idx] = dialFromJson(it.value().toObject());
    }

    l.pages.push_back(std::move(p));
  }
  return l;
}
}
