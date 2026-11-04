cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

if(NOT TARGET score_lib_base)
  include(ScoreExternalAddon)
endif()

project(score_addon_streamdeck LANGUAGES CXX)

if(NOT TARGET score_plugin_gfx)
  return()
endif()

if(NOT TARGET score_plugin_js)
  return()
endif()

if(DEFINED OSSIA_PROTOCOL_HID AND NOT OSSIA_PROTOCOL_HID)
  return()
endif()

score_common_setup()

set(HDRS
  StreamDeck/ApplicationPlugin.hpp
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
  StreamDeck/Script.hpp
  StreamDeck/SpecificSettings.hpp
  StreamDeck/Editor/ActionList.hpp
  StreamDeck/Editor/ButtonEditor.hpp
  StreamDeck/Editor/ColorButton.hpp
  StreamDeck/Editor/DeckView.hpp
  StreamDeck/Editor/Editor.hpp
  StreamDeck/Editor/WheelGuard.hpp
  score_addon_streamdeck.hpp
)

set(SRCS
  StreamDeck/ApplicationPlugin.cpp
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
  StreamDeck/Script.cpp
  StreamDeck/SpecificSettingsSerialization.cpp
  StreamDeck/Editor/ActionList.cpp
  StreamDeck/Editor/ButtonEditor.cpp
  StreamDeck/Editor/ColorButton.cpp
  StreamDeck/Editor/DeckView.cpp
  StreamDeck/Editor/Editor.cpp
  score_addon_streamdeck.cpp
)

add_library(score_addon_streamdeck ${SRCS} ${HDRS})
score_generate_command_list_file(score_addon_streamdeck "${HDRS}")

target_link_libraries(score_addon_streamdeck PUBLIC score_plugin_engine score_plugin_gfx score_plugin_js)

setup_score_plugin(score_addon_streamdeck)

if(SCORE_TESTING AND COMMAND score_add_test)
  score_add_test(score_addon_streamdeck_models GUI STANDALONE
    SOURCES
      tests/ModelTests.cpp
      StreamDeck/Model.cpp
      StreamDeck/Render.cpp
    LIBS
      ${QT_PREFIX}::Gui
  )
  target_include_directories(score_addon_streamdeck_models PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    $<TARGET_PROPERTY:ossia,INTERFACE_INCLUDE_DIRECTORIES>)
endif()
