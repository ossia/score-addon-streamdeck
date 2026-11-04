#pragma once
#include <score/command/Command.hpp>

#include <StreamDeck/SpecificSettings.hpp>

namespace StreamDeck
{
const CommandGroupKey& CommandFactoryName();

//! Changes the settings of a Stream Deck device, its layout included, without
//! reconnecting it.
class EditDeck final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), EditDeck, "Edit Stream Deck")
public:
  EditDeck(QString device, SpecificSettings before, SpecificSettings after);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  QString m_device;
  SpecificSettings m_before;
  SpecificSettings m_after;
};
}
