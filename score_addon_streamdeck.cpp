#include "score_addon_streamdeck.hpp"

#include <score/plugins/FactorySetup.hpp>

#include <StreamDeck/ProtocolFactory.hpp>

score_addon_streamdeck::score_addon_streamdeck() { }

score_addon_streamdeck::~score_addon_streamdeck() { }

std::vector<score::InterfaceBase*> score_addon_streamdeck::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Device::ProtocolFactory, StreamDeck::ProtocolFactory>>(ctx, key);
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_addon_streamdeck)
