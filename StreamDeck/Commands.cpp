#include "Commands.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <score/model/path/PathSerialization.hpp>

#include <StreamDeck/Device.hpp>

namespace StreamDeck
{
const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"StreamDeck"};
  return key;
}

namespace
{
void apply(const score::DocumentContext& ctx, const QString& name, const SpecificSettings& s)
{
  auto& plug = ctx.plugin<Explorer::DeviceDocumentPlugin>();
  auto dev = dynamic_cast<DeckDevice*>(plug.list().findDevice(name));
  if(!dev)
    return;
  dev->applySettings(s);
  plug.explorer().updateDevice(name, dev->settings());
}
}

EditDeck::EditDeck(QString device, SpecificSettings before, SpecificSettings after)
    : m_device{std::move(device)}
    , m_before{std::move(before)}
    , m_after{std::move(after)}
{
}

void EditDeck::undo(const score::DocumentContext& ctx) const
{
  apply(ctx, m_device, m_before);
}

void EditDeck::redo(const score::DocumentContext& ctx) const
{
  apply(ctx, m_device, m_after);
}

void EditDeck::serializeImpl(DataStreamInput& s) const
{
  s << m_device << m_before << m_after;
}

void EditDeck::deserializeImpl(DataStreamOutput& s)
{
  s >> m_device >> m_before >> m_after;
}
}
