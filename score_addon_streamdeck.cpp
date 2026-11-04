#include "score_addon_streamdeck.hpp"

#include <score/command/CommandGeneratorMap.hpp>
#include <score/plugins/FactorySetup.hpp>

#include <ossia/detail/for_each.hpp>

#include <StreamDeck/ApplicationPlugin.hpp>
#include <StreamDeck/Commands.hpp>
#include <StreamDeck/ProtocolFactory.hpp>

#include <score_addon_streamdeck_commands_files.hpp>

score_addon_streamdeck::score_addon_streamdeck() { }

score_addon_streamdeck::~score_addon_streamdeck() { }

std::vector<score::InterfaceBase*> score_addon_streamdeck::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Device::ProtocolFactory, StreamDeck::ProtocolFactory>>(ctx, key);
}

score::GUIApplicationPlugin*
score_addon_streamdeck::make_guiApplicationPlugin(const score::GUIApplicationContext& app)
{
  return new StreamDeck::ApplicationPlugin{app};
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
score_addon_streamdeck::make_commands()
{
  using namespace StreamDeck;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      StreamDeck::CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_addon_streamdeck_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_addon_streamdeck)
