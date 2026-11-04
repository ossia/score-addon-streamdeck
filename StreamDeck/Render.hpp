#pragma once
#include <StreamDeck/Layout.hpp>

#include <QHash>
#include <QImage>
#include <QSize>

namespace StreamDeck
{
//! Decoded images by path. Not thread-safe: one per thread.
class ImageCache
{
public:
  const QImage& get(const QString& path);

private:
  QHash<QString, QImage> m_images;
};

QImage renderStyle(const Style& s, QSize size, ImageCache& cache);
}
