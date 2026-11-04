#include "SpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const StreamDeck::SpecificSettings& n)
{
  m_stream << n.path << n.serial << n.productId << n.mode << n.brightness << n.rate;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(StreamDeck::SpecificSettings& n)
{
  m_stream >> n.path >> n.serial >> n.productId >> n.mode >> n.brightness >> n.rate;
  checkDelimiter();
}

template <>
void JSONReader::read(const StreamDeck::SpecificSettings& n)
{
  obj["Path"] = n.path;
  obj["Serial"] = n.serial;
  obj["ProductId"] = n.productId;
  obj["Mode"] = n.mode;
  obj["Brightness"] = n.brightness;
  obj["Rate"] = n.rate;
}

template <>
void JSONWriter::write(StreamDeck::SpecificSettings& n)
{
  n.path = obj["Path"].toString();
  n.serial = obj["Serial"].toString();
  n.productId = obj["ProductId"].toInt();
  if(auto v = obj.tryGet("Mode"))
    n.mode = v->toInt();
  if(auto v = obj.tryGet("Brightness"))
    n.brightness = v->toInt();
  if(auto v = obj.tryGet("Rate"))
    n.rate = v->toDouble();
}
