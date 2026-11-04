#pragma once
#include <Gfx/Graph/OutputNode.hpp>

#include <QSize>

#include <memory>

class QRhiTexture;
class QRhiTextureRenderTarget;

namespace StreamDeck
{
class FrameSink;

//! Renders what is connected to the device's root into a texture of the
//! deck's canvas size, and hands each frame to the device's frame sink.
struct OutputNode final : score::gfx::OutputNode
{
  OutputNode(std::shared_ptr<FrameSink> sink, QSize canvas, double rate);
  ~OutputNode() override;

  void startRendering() override;
  void onRendererChange() override;
  void render() override;
  bool canRender() const override;
  void stopRendering() override;

  void setRenderer(std::shared_ptr<score::gfx::RenderList> r) override;
  score::gfx::RenderList* renderer() const override;

  void createOutput(score::gfx::OutputConfiguration conf) override;
  void destroyOutput() override;

  std::shared_ptr<score::gfx::RenderState> renderState() const override;
  score::gfx::OutputNodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override;
  Configuration configuration() const noexcept override;

private:
  std::shared_ptr<FrameSink> m_sink;
  QSize m_canvas;
  double m_rate{};
  bool m_enabled{};

  std::weak_ptr<score::gfx::RenderList> m_renderer{};
  QRhiTexture* m_texture{};
  QRhiTextureRenderTarget* m_renderTarget{};
  std::shared_ptr<score::gfx::RenderState> m_renderState{};
  QRhiReadbackResult m_readback;
};
}
