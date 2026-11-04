#include "RemoteControl.hpp"

#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>

#include <ossia/detail/flicks.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <StreamDeck/Engine.hpp>

namespace StreamDeck
{
namespace
{
const QColor actionBackground{0x1d, 0x3a, 0x5f};
const QColor controlBackground{0x22, 0x22, 0x22};
const QColor activeBackground{0x2f, 0x7d, 0x32};

QString formatValue(const ossia::value& v)
{
  struct
  {
    QString operator()(ossia::impulse) const { return QStringLiteral("●"); }
    QString operator()(int i) const { return QString::number(i); }
    QString operator()(float f) const { return QString::number(f, 'f', 2); }
    QString operator()(bool b) const { return b ? QStringLiteral("on") : QStringLiteral("off"); }
    QString operator()(const std::string& s) const { return QString::fromStdString(s); }
    QString operator()(const ossia::vec2f& f) const
    {
      return QStringLiteral("%1 %2").arg(f[0], 0, 'f', 2).arg(f[1], 0, 'f', 2);
    }
    QString operator()(const ossia::vec3f& f) const
    {
      return QStringLiteral("%1 %2 %3")
          .arg(f[0], 0, 'f', 1)
          .arg(f[1], 0, 'f', 1)
          .arg(f[2], 0, 'f', 1);
    }
    QString operator()(const ossia::vec4f& f) const { return QStringLiteral("…"); }
    QString operator()(const std::vector<ossia::value>& l) const
    {
      return QStringLiteral("[%1]").arg(l.size());
    }
    QString operator()(const ossia::value_map_type& l) const { return QStringLiteral("{…}"); }
    QString operator()() const { return {}; }
  } vis;
  return v.apply(vis);
}
}

RemoteControl::RemoteControl(const score::DocumentContext& doc, Engine& engine)
    : m_doc{doc}
    , m_engine{engine}
{
  auto& providers = score::AppContext().interfaces<Process::RemoteControlProviderList>();
  if(providers.empty())
    return;
  m_rc = providers.begin()->make(doc);
  if(!m_rc)
    return;

  layout();

  connect(
      m_rc.get(), &Process::RemoteControlInterface::controlNameChanged, this,
      &RemoteControl::onNameChanged);
  connect(
      m_rc.get(), &Process::RemoteControlInterface::controlValueChanged, this,
      &RemoteControl::onValueChanged);
  connect(
      m_rc.get(), &Process::RemoteControlInterface::transportChanged, this,
      [this](ossia::time_value t, double, double, double, double) { onTransport(t); });
}

RemoteControl::~RemoteControl()
{
  if(!m_rc)
    return;
  auto& providers = score::AppContext().interfaces<Process::RemoteControlProviderList>();
  if(!providers.empty())
    providers.begin()->release(m_doc, m_rc);
}

void RemoteControl::layout()
{
  using hint = Process::RemoteControlInterface::ControllerHint;
  const auto& model = m_engine.model();
  const int keys = model.keyCount();

  m_actions.assign(keys, Action::Control);
  if(model.rows > 1 || !model.hasDisplay())
  {
    std::vector<Action> transport;
    if(model.columns >= 8)
      transport = {Action::Play, Action::Stop, Action::PreviousBank, Action::NextBank,
                   Action::Undo, Action::Redo, Action::None, Action::Time};
    else if(model.columns >= 5)
      transport = {Action::Play, Action::Stop, Action::PreviousBank, Action::NextBank,
                   Action::Time};
    else if(model.columns == 4)
      transport = {Action::Play, Action::Stop, Action::PreviousBank, Action::NextBank};
    else
      transport = {Action::Play, Action::Stop, Action::NextBank};

    const int first = (model.rows - 1) * model.columns;
    for(int i = 0; i < model.columns && i < int(transport.size()); i++)
      m_actions[first + i] = transport[i];
    for(int i = transport.size(); i < model.columns; i++)
      m_actions[first + i] = Action::None;
  }

  if(model.hasLcd())
  {
    const auto handles = m_rc->registerControllerGroup(
        (hint)(hint::Knob | hint::MapControls), model.encoders);
    for(int i = 0; i < int(handles.size()); i++)
      m_encoderControls.push_back({.handle = handles[i], .face = keys + i});
  }

  const int controlKeys = std::count(m_actions.begin(), m_actions.end(), Action::Control);
  const auto handles = m_rc->registerControllerGroup(
      (hint)(hint::Button_Function | hint::MapControls), controlKeys);

  m_keyToControl.assign(keys, -1);
  int next = 0;
  for(int i = 0; i < keys; i++)
  {
    if(m_actions[i] == Action::Control && next < int(handles.size()))
    {
      m_keyToControl[i] = m_keyControls.size();
      m_keyControls.push_back({.handle = handles[next++], .face = i});
    }
    else if(m_actions[i] == Action::Time)
    {
      m_timeKey = i;
    }
  }

  for(int i = 0; i < m_engine.faceCount(); i++)
    m_engine.display(i, Style{});
  for(int i = 0; i < keys; i++)
    drawAction(i, m_actions[i]);
  for(auto& c : m_keyControls)
    drawControl(c);
  for(auto& c : m_encoderControls)
    drawControl(c);
}

void RemoteControl::drawAction(int key, Action a)
{
  Style f;
  f.background = actionBackground;
  switch(a)
  {
    case Action::Play:
      f.text = QStringLiteral("▶");
      f.textSize = m_engine.model().keyHeight / 2;
      break;
    case Action::Stop:
      f.text = QStringLiteral("■");
      f.textSize = m_engine.model().keyHeight / 2;
      break;
    case Action::PreviousBank:
      f.text = QStringLiteral("◀ bank");
      break;
    case Action::NextBank:
      f.text = QStringLiteral("bank ▶");
      break;
    case Action::Undo:
      f.text = QStringLiteral("undo");
      break;
    case Action::Redo:
      f.text = QStringLiteral("redo");
      break;
    case Action::Time:
      f.background = Qt::black;
      f.text = QStringLiteral("0:00.0");
      break;
    case Action::None:
    case Action::Control:
      return;
  }
  m_engine.display(key, f);
}

void RemoteControl::drawControl(const Mapped& m, bool highlight)
{
  Style f;
  if(m.name.isEmpty())
  {
    m_engine.display(m.face, f);
    return;
  }

  const bool on = m.value.target<bool>() && *m.value.target<bool>();
  f.background = highlight || on ? activeBackground : controlBackground;
  f.text = m.name + QStringLiteral("\n") + formatValue(m.value);
  m_engine.display(m.face, f);
}

RemoteControl::Mapped* RemoteControl::find(Handle h)
{
  for(auto& c : m_keyControls)
    if(c.handle == h)
      return &c;
  for(auto& c : m_encoderControls)
    if(c.handle == h)
      return &c;
  return nullptr;
}

void RemoteControl::onNameChanged(Handle h, QString name)
{
  if(auto m = find(h))
  {
    m->name = std::move(name);
    if(m->name.isEmpty())
      m->value = ossia::value{};
    drawControl(*m);
  }
}

void RemoteControl::onValueChanged(Handle h, ossia::value v)
{
  if(auto m = find(h))
  {
    m->value = std::move(v);
    drawControl(*m);
  }
}

void RemoteControl::onTransport(ossia::time_value t)
{
  if(m_timeKey < 0)
    return;
  const double secs = double(t.impl) / ossia::flicks_per_second<double>;
  const int minutes = int(secs / 60.);
  const double rest = secs - minutes * 60.;
  auto text = QStringLiteral("%1:%2").arg(minutes).arg(rest, 4, 'f', 1, QChar('0'));
  if(text == m_timeText)
    return;
  m_timeText = text;
  auto face = m_engine.face(m_timeKey);
  face.text = text;
  m_engine.display(m_timeKey, face);
}

void RemoteControl::onKey(int key, bool pressed)
{
  if(!m_rc || key < 0 || key >= int(m_actions.size()))
    return;

  using act = Process::RemoteControlInterface::ControllerAction;
  const auto a = pressed ? act::Press : act::Release;
  switch(m_actions[key])
  {
    case Action::Play:
      if(pressed)
        m_rc->play(a);
      break;
    case Action::Stop:
      if(pressed)
        m_rc->stop(a);
      break;
    case Action::PreviousBank:
      m_rc->prevBank(a);
      break;
    case Action::NextBank:
      m_rc->nextBank(a);
      break;
    case Action::Undo:
      if(pressed)
        m_rc->undo(a);
      break;
    case Action::Redo:
      if(pressed)
        m_rc->redo(a);
      break;
    case Action::Control: {
      const int idx = m_keyToControl[key];
      if(idx < 0)
        break;
      auto& m = m_keyControls[idx];
      if(m.name.isEmpty())
        break;

      drawControl(m, pressed);
      if(!pressed)
        break;

      switch(m.value.get_type())
      {
        case ossia::val_type::BOOL:
          m_rc->setControl(m.handle, !*m.value.target<bool>());
          break;
        case ossia::val_type::IMPULSE:
          m_rc->setControl(m.handle, ossia::impulse{});
          break;
        default:
          m_rc->offsetControl(m.handle, 10.);
          break;
      }
      break;
    }
    case Action::Time:
    case Action::None:
      break;
  }
}

void RemoteControl::onEncoderPress(int encoder, bool pressed)
{
  if(!m_rc || !pressed || encoder < 0 || encoder >= int(m_encoderControls.size()))
    return;
  auto& m = m_encoderControls[encoder];
  if(m.value.get_type() == ossia::val_type::BOOL)
    m_rc->setControl(m.handle, !*m.value.target<bool>());
  else if(m.value.get_type() == ossia::val_type::IMPULSE)
    m_rc->setControl(m.handle, ossia::impulse{});
}

void RemoteControl::onEncoderTurn(int encoder, int delta)
{
  if(!m_rc || encoder < 0 || encoder >= int(m_encoderControls.size()))
    return;
  m_rc->offsetControl(m_encoderControls[encoder].handle, delta);
}
}
