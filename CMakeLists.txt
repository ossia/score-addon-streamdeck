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

add_library(score_addon_streamdeck
  StreamDeck/Deck.cpp
  StreamDeck/Deck.hpp
  StreamDeck/Device.cpp
  StreamDeck/Device.hpp
  StreamDeck/Model.cpp
  StreamDeck/Model.hpp
  StreamDeck/OutputNode.cpp
  StreamDeck/OutputNode.hpp
  StreamDeck/Protocol.cpp
  StreamDeck/Protocol.hpp
  StreamDeck/ProtocolFactory.cpp
  StreamDeck/ProtocolFactory.hpp
  StreamDeck/ProtocolSettingsWidget.cpp
  StreamDeck/ProtocolSettingsWidget.hpp
  StreamDeck/RemoteControl.cpp
  StreamDeck/RemoteControl.hpp
  StreamDeck/SpecificSettings.hpp
  StreamDeck/SpecificSettingsSerialization.cpp

  score_addon_streamdeck.hpp
  score_addon_streamdeck.cpp
)

target_link_libraries(score_addon_streamdeck PUBLIC score_plugin_engine score_plugin_gfx)

setup_score_plugin(score_addon_streamdeck)
