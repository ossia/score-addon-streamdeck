cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

if(NOT TARGET score_lib_base)
  include(ScoreExternalAddon)
endif()

project(score_addon_streamdeck LANGUAGES CXX)

if(NOT TARGET score_plugin_gfx)
  return()
endif()

if(DEFINED OSSIA_PROTOCOL_HID AND NOT OSSIA_PROTOCOL_HID)
  return()
endif()

score_common_setup()

set(HDRS
  StreamDeck/Commands.hpp
  StreamDeck/Deck.hpp
  StreamDeck/Device.hpp
  StreamDeck/Engine.hpp
  StreamDeck/FrameSink.hpp
  StreamDeck/Layout.hpp
  StreamDeck/Model.hpp
  StreamDeck/OutputNode.hpp
  StreamDeck/Protocol.hpp
  StreamDeck/ProtocolFactory.hpp
  StreamDeck/ProtocolSettingsWidget.hpp
  StreamDeck/RemoteControl.hpp
  StreamDeck/Render.hpp
  StreamDeck/SpecificSettings.hpp
  StreamDeck/Panel/ActionList.hpp
  StreamDeck/Panel/ButtonEditor.hpp
  StreamDeck/Panel/ColorButton.hpp
  StreamDeck/Panel/DeckView.hpp
  StreamDeck/Panel/Editor.hpp
  StreamDeck/Panel/Panel.hpp
  StreamDeck/Panel/WheelGuard.hpp
  score_addon_streamdeck.hpp
)

set(SRCS
  StreamDeck/Commands.cpp
  StreamDeck/Deck.cpp
  StreamDeck/Device.cpp
  StreamDeck/Engine.cpp
  StreamDeck/FrameSink.cpp
  StreamDeck/Layout.cpp
  StreamDeck/Model.cpp
  StreamDeck/OutputNode.cpp
  StreamDeck/Protocol.cpp
  StreamDeck/ProtocolFactory.cpp
  StreamDeck/ProtocolSettingsWidget.cpp
  StreamDeck/RemoteControl.cpp
  StreamDeck/Render.cpp
  StreamDeck/SpecificSettingsSerialization.cpp
  StreamDeck/Panel/ActionList.cpp
  StreamDeck/Panel/ButtonEditor.cpp
  StreamDeck/Panel/ColorButton.cpp
  StreamDeck/Panel/DeckView.cpp
  StreamDeck/Panel/Editor.cpp
  StreamDeck/Panel/Panel.cpp
  score_addon_streamdeck.cpp
)

add_library(score_addon_streamdeck ${SRCS} ${HDRS})
score_generate_command_list_file(score_addon_streamdeck "${HDRS}")

target_link_libraries(score_addon_streamdeck PUBLIC score_plugin_engine score_plugin_gfx)

setup_score_plugin(score_addon_streamdeck)
