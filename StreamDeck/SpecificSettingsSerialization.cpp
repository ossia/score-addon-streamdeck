#include "SpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <QJsonDocument>

namespace
{
QByteArray layoutBytes(const StreamDeck::Layout& l)
{
  return QJsonDocument{StreamDeck::toJson(l)}.toJson(QJsonDocument::Compact);
}

StreamDeck::Layout layoutFromBytes(const QByteArray& b)
{
  return StreamDeck::layoutFromJson(QJsonDocument::fromJson(b).object());
}
}

template <>
void DataStreamReader::read(const StreamDeck::SpecificSettings& n)
{
  m_stream << n.path << n.serial << n.vendorId << n.productId << n.mode << n.brightness
           << n.rate << layoutBytes(n.layout);
  insertDelimiter();
}

template <>
void DataStreamWriter::write(StreamDeck::SpecificSettings& n)
{
  QByteArray layout;
  m_stream >> n.path >> n.serial >> n.vendorId >> n.productId >> n.mode >> n.brightness
      >> n.rate >> layout;
  n.layout = layoutFromBytes(layout);
  checkDelimiter();
}

template <>
void JSONReader::read(const StreamDeck::SpecificSettings& n)
{
  obj["Path"] = n.path;
  obj["Serial"] = n.serial;
  obj["VendorId"] = n.vendorId;
  obj["ProductId"] = n.productId;
  obj["Mode"] = n.mode;
  obj["Brightness"] = n.brightness;
  obj["Rate"] = n.rate;
  obj["Layout"] = QString::fromUtf8(layoutBytes(n.layout));
}

template <>
void JSONWriter::write(StreamDeck::SpecificSettings& n)
{
  n.path = obj["Path"].toString();
  n.serial = obj["Serial"].toString();
  n.vendorId = StreamDeck::elgato_vendor_id;
  if(auto v = obj.tryGet("VendorId"))
    n.vendorId = v->toInt();
  n.productId = obj["ProductId"].toInt();
  if(auto v = obj.tryGet("Mode"))
    n.mode = v->toInt();
  if(auto v = obj.tryGet("Brightness"))
    n.brightness = v->toInt();
  if(auto v = obj.tryGet("Rate"))
    n.rate = v->toDouble();
  if(auto v = obj.tryGet("Layout"))
    n.layout = layoutFromBytes(v->toString().toUtf8());
}
