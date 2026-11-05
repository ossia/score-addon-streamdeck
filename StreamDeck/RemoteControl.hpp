#pragma once
#include <RemoteControl/RemoteControlProvider.hpp>

#include <ossia/network/value/value.hpp>

#include <QObject>

#include <memory>
#include <vector>

namespace score
{
struct DocumentContext;
}

namespace StreamDeck
{
class Engine;

class RemoteControl final : public QObject
{
public:
  RemoteControl(const score::DocumentContext& doc, Engine& engine);
  ~RemoteControl() override;

  //! Main thread only.
  void onKey(int key, bool pressed);
  void onEncoderPress(int encoder, bool pressed);
  void onEncoderTurn(int encoder, int delta);

private:
  using Handle = Process::RemoteControlInterface::ControllerHandle;
  enum class Action
  {
    None,
    Play,
    Stop,
    PreviousBank,
    NextBank,
    Undo,
    Redo,
    Time,
    Control
  };

  struct Mapped
  {
    Handle handle{};
    int face{-1};
    QString name;
    ossia::value value;
  };

  void layout();
  void drawAction(int key, Action a);
  void drawControl(const Mapped& m, bool highlight = false);
  void onNameChanged(Handle h, QString name);
  void onValueChanged(Handle h, ossia::value v);
  void onTransport(ossia::time_value t);
  Mapped* find(Handle h);

  const score::DocumentContext& m_doc;
  Engine& m_engine;
  std::shared_ptr<Process::RemoteControlInterface> m_rc;

  std::vector<Action> m_actions;
  std::vector<Mapped> m_keyControls;
  std::vector<Mapped> m_encoderControls;
  std::vector<int> m_keyToControl;
  int m_timeKey{-1};
  QString m_timeText;
};
}
