#pragma once
#include <StreamDeck/Layout.hpp>
#include <StreamDeck/Model.hpp>

#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QRect>
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

//! Keys, then one face per segment of the touch strip.
int faceCount(const Model& m) noexcept;
QSize faceSize(const Model& m, int face) noexcept;

//! The size of the texture the keys of a model are cut from, and where each
//! face sits in it.
QSize canvasSize(const Model& m) noexcept;
QRect tileRect(const Model& m, int face) noexcept;

QImage renderStyle(const Style& s, QSize size, ImageCache& cache);

//! A key image as the model expects it: rotated, flipped.
QImage orient(const QImage& img, const Model& m);
//! JPEG quality goes down until the image fits in maxBytes, if positive.
QByteArray encode(const QImage& img, ImageFormat format, int maxBytes = 0);
}
