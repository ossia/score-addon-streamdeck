#include "OutputNode.hpp"

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/InvertYRenderer.hpp>

#include <StreamDeck/FrameSink.hpp>

namespace StreamDeck
{
OutputNode::OutputNode(std::shared_ptr<FrameSink> sink, QSize canvas, double rate)
    : m_sink{std::move(sink)}
    , m_canvas{canvas.isEmpty() ? QSize{1, 1} : canvas}
    , m_rate{rate}
    , m_enabled{!canvas.isEmpty()}
{
  input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
}

OutputNode::~OutputNode() { }

bool OutputNode::canRender() const
{
  return m_enabled && bool(m_renderState);
}

void OutputNode::startRendering() { }
void OutputNode::onRendererChange() { }
void OutputNode::stopRendering() { }

void OutputNode::render()
{
  if(m_deviceLost)
    return;
  auto renderer = m_renderer.lock();
  if(!renderer || !m_renderState)
    return;

  auto rhi = m_renderState->rhi;
  score::gfx::OffscreenFrame frame{*rhi};
  if(!frame)
  {
    checkDeviceLost(frame, *rhi, "StreamDeck::OutputNode");
    return;
  }

  renderer->render(frame.commands());
  frame.end();
  if(checkDeviceLost(frame, *rhi, "StreamDeck::OutputNode"))
    return;

  const auto sz = m_readback.pixelSize;
  if(m_readback.data.size() < sz.width() * sz.height() * 4)
    return;

  m_sink->submit(
      reinterpret_cast<const uint8_t*>(m_readback.data.constData()), sz.width(),
      sz.height(), sz.width() * 4);
}

score::gfx::OutputNode::Configuration OutputNode::configuration() const noexcept
{
  return {.manualRenderingRate = 1000. / std::max(1., m_rate)};
}

void OutputNode::setRenderer(std::shared_ptr<score::gfx::RenderList> r)
{
  m_renderer = r;
}

score::gfx::RenderList* OutputNode::renderer() const
{
  return m_renderer.lock().get();
}

void OutputNode::createOutput(score::gfx::OutputConfiguration conf)
{
  resetDeviceLost();

  m_renderState = score::gfx::createRenderState(conf.graphicsApi, m_canvas, nullptr);
  if(!m_renderState || !m_renderState->rhi)
  {
    qWarning() << "StreamDeck: failed to create QRhi";
    m_renderState.reset();
    return;
  }
  m_renderState->outputSize = m_renderState->renderSize;

  auto rhi = m_renderState->rhi;
  m_texture = rhi->newTexture(
      QRhiTexture::RGBA8, m_renderState->renderSize, 1,
      QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
  m_texture->create();
  m_renderTarget = rhi->newTextureRenderTarget({m_texture});
  m_renderState->renderPassDescriptor = m_renderTarget->newCompatibleRenderPassDescriptor();
  m_renderTarget->setRenderPassDescriptor(m_renderState->renderPassDescriptor);
  m_renderTarget->create();

  conf.onReady();
}

void OutputNode::destroyOutput()
{
  if(!m_renderState)
    return;

  releaseRegistry();

  delete m_renderTarget;
  m_renderTarget = nullptr;

  delete m_renderState->renderPassDescriptor;
  m_renderState->renderPassDescriptor = nullptr;

  delete m_texture;
  m_texture = nullptr;

  m_renderState->destroy();
  m_renderState.reset();
}

std::shared_ptr<score::gfx::RenderState> OutputNode::renderState() const
{
  return m_renderState;
}

score::gfx::OutputNodeRenderer*
OutputNode::createRenderer(score::gfx::RenderList& r) const noexcept
{
  score::gfx::TextureRenderTarget rt{
      .texture = m_texture,
      .renderPass = m_renderState->renderPassDescriptor,
      .renderTarget = m_renderTarget};
  return new Gfx::InvertYRenderer{
      *this, rt, const_cast<QRhiReadbackResult&>(m_readback)};
}
}
