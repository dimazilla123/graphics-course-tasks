#pragma once

#include <etna/Window.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/Image.hpp>

#include <etna/Buffer.hpp>
#include <etna/Sampler.hpp>
#include <wsi/OsWindowingManager.hpp>


class App
{
public:
  App();
  ~App();

  void run();

private:
  void drawFrame();

  void initGen();
  void initToy();
  void pushUniformConstants(
    vk::CommandBuffer& current_cmd_buf,
    etna::GraphicsPipeline& pipeline,
    vk::ShaderStageFlagBits flags);
  etna::Image loadTexture(const std::string_view& path, const std::string_view& tex_name);
  void prepareGen(
    vk::CommandBuffer& cmd_buffer, vk::Image& backbuffer, vk::ImageView& backbuffer_view);
  void prepareToy(
    vk::CommandBuffer& cmd_buffer, vk::Image& backbuffer, vk::ImageView& backbuffer_view);

private:
  OsWindowingManager windowing;
  std::unique_ptr<OsWindow> osWindow;

  glm::uvec2 resolution;
  bool useVsync;

  std::unique_ptr<etna::Window> vkWindow;
  std::unique_ptr<etna::PerFrameCmdMgr> commandManager;

  etna::GraphicsPipeline toyPipeline;
  etna::GraphicsPipeline genPipeline;
  etna::Sampler defaultSampler;

  etna::Image torusTex;
  etna::Image skyboxTex;
  etna::Image generatedTex;

  etna::Buffer constants;
};
