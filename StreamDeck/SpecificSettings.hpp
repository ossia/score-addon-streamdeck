#pragma once
#include <StreamDeck/Layout.hpp>
#include <StreamDeck/Model.hpp>

#include <QString>

#include <verdigris>

namespace StreamDeck
{
struct SpecificSettings
{
  enum Mode : int
  {
    Buttons = 0,
    RemoteControl = 1
  };

  QString path;
  QString serial;
  int vendorId{elgato_vendor_id};
  int productId{};
  int mode{Buttons};
  int brightness{70};
  double rate{30.};
  Layout layout;

  //! Whether these settings open another deck, or another kind of deck.
  bool sameDeck(const SpecificSettings& o) const noexcept
  {
    return path == o.path && serial == o.serial && vendorId == o.vendorId
           && productId == o.productId && rate == o.rate;
  }
};
}

Q_DECLARE_METATYPE(StreamDeck::SpecificSettings)
W_REGISTER_ARGTYPE(StreamDeck::SpecificSettings)
