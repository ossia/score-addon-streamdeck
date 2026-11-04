#pragma once
#include <QString>

#include <verdigris>

namespace StreamDeck
{
struct SpecificSettings
{
  enum Mode : int
  {
    Parameters = 0,
    RemoteControl = 1
  };

  QString path;
  QString serial;
  int productId{};
  int mode{Parameters};
  int brightness{70};
  double rate{30.};
};
}

Q_DECLARE_METATYPE(StreamDeck::SpecificSettings)
W_REGISTER_ARGTYPE(StreamDeck::SpecificSettings)
