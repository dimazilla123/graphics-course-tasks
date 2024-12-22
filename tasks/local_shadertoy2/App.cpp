#include "App.hpp"
#include "etna/DescriptorSet.hpp"
#include "etna/GraphicsPipeline.hpp"
#include "etna/Image.hpp"
#include "etna/RenderTargetStates.hpp"
#include "etna/Sampler.hpp"
#include "etna/VertexInput.hpp"

#include <cstdint>
#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <iostream>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>


App::App()
  : resolution{1280, 720}
  , useVsync{true}
{
  // First, we need to initialize Vulkan, which is not trivial because
  // extensions are required for just about anything.
  {
    // GLFW tells us which extensions it needs to present frames to the OS window.
    // Actually rendering anything to a screen is optional in Vulkan, you can
    // alternatively save rendered frames into files, send them over network, etc.
    // Instance extensions do not depend on the actual GPU, only on the OS.
    auto glfwInstExts = windowing.getRequiredVulkanInstanceExtensions();

    std::vector<const char*> instanceExtensions{glfwInstExts.begin(), glfwInstExts.end()};

    // We also need the swapchain device extension to get access to the OS
    // window from inside of Vulkan on the GPU.
    // Device extensions require HW support from the GPU.
    // Generally, in Vulkan, we call the GPU a "device" and the CPU/OS combination a "host."
    std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    // Etna does all of the Vulkan initialization heavy lifting.
    // You can skip figuring out how it works for now.
    etna::initialize(etna::InitParams{
      .applicationName = "Local Shadertoy",
      .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
      .instanceExtensions = instanceExtensions,
      .deviceExtensions = deviceExtensions,
      // Replace with an index if etna detects your preferred GPU incorrectly
      .physicalDeviceIndexOverride = {},
      .numFramesInFlight = 1,
    });
  }

  // Now we can create an OS window
  osWindow = windowing.createWindow(OsWindow::CreateInfo{
    .resolution = resolution,
  });

  // But we also need to hook the OS window up to Vulkan manually!
  {
    // First, we ask GLFW to provide a "surface" for the window,
    // which is an opaque description of the area where we can actually render.
    auto surface = osWindow->createVkSurface(etna::get_context().getInstance());

    // Then we pass it to Etna to do the complicated work for us
    vkWindow = etna::get_context().createWindow(etna::Window::CreateInfo{
      .surface = std::move(surface),
    });

    // And finally ask Etna to create the actual swapchain so that we can
    // get (different) images each frame to render stuff into.
    // Here, we do not support window resizing, so we only need to call this once.
    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });

    // Technically, Vulkan might fail to initialize a swapchain with the requested
    // resolution and pick a different one. This, however, does not occur on platforms
    // we support. Still, it's better to follow the "intended" path.
    resolution = {w, h};
  }

  // Next, we need a magical Etna helper to send commands to the GPU.
  // How it is actually performed is not trivial, but we can skip this for now.
  commandManager = etna::get_context().createPerFrameCmdMgr();
  initGen();
  initToy();
  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});
}

void App::initGen()
{
  auto& ctx = etna::get_context();
  auto& pipelineManager = ctx.getPipelineManager();

  etna::create_program("gen", {
    LOCAL_SHADERTOY2_SHADERS_ROOT "rect.vert.spv",
    LOCAL_SHADERTOY2_SHADERS_ROOT "gen.frag.spv",
  });

  genPipeline = pipelineManager.createGraphicsPipeline("gen", {
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput =
      {
        .colorAttachmentFormats = {vk::Format::eB8G8R8A8Srgb},
      }
    }
  });

  etna::Image::CreateInfo genTexInfo {
    .extent = vk::Extent3D {
      .width = 256,
      .height = 256,
      .depth = 1,
    },
    .name = "generatedTex",
    .format = vk::Format::eB8G8R8A8Srgb,
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment,
  };
  generatedTex = etna::get_context().createImage(genTexInfo);
}

void App::initToy()
{
  auto& ctx = etna::get_context();
  auto& pipelineManager = ctx.getPipelineManager();

  etna::create_program("toy", {
    LOCAL_SHADERTOY2_SHADERS_ROOT "rect.vert.spv",
    LOCAL_SHADERTOY2_SHADERS_ROOT "toy.frag.spv",
  });

  toyPipeline = {};
  toyPipeline = pipelineManager.createGraphicsPipeline("toy", {
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput = {
        .colorAttachmentFormats = {vk::Format::eB8G8R8A8Srgb},
      }
    }
  });

}

App::~App()
{
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::run()
{
  while (!osWindow->isBeingClosed())
  {
    windowing.poll();

    drawFrame();
  }

  // We need to wait for the GPU to execute the last frame before destroying
  // all resources and closing the application.
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::pushUniformConstants(vk::CommandBuffer& current_cmd_buf, etna::GraphicsPipeline &pipeline, vk::ShaderStageFlagBits flags) {
  static const std::chrono::time_point INITIAL_TIME = std::chrono::high_resolution_clock::now();
  std::chrono::time_point now = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(INITIAL_TIME - now);
  (void)duration;
  struct
  {
    struct
    {
      uint32_t x;
      uint32_t y;
    } resolution;
    float time;
  } params = {{resolution.x, resolution.y}, duration.count() / 1000.f};

  current_cmd_buf.pushConstants(
    pipeline.getVkPipelineLayout(),
    flags,
    0,
    sizeof(params),
    &params);

}

void App::prepareGen(vk::CommandBuffer& current_cmd_buf, vk::Image& backbuffer, vk::ImageView& backbuffer_view)
{
  // auto genComputeInfo = etna::get_shader_program("gen");
  etna::set_state(
    current_cmd_buf,
    generatedTex.get(),
    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    {vk::AccessFlagBits2::eColorAttachmentWrite},
    vk::ImageLayout::eColorAttachmentOptimal,
    vk::ImageAspectFlagBits::eColor
  );

  etna::RenderTargetState state{
    current_cmd_buf,
    {{}, {256, 256}},
    {{backbuffer, backbuffer_view}},
    {}
  };
  current_cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, genPipeline.getVkPipeline());
  // auto set = etna::create_descriptor_set(
  //   genComputeInfo.getDescriptorLayoutId(0),
  //   current_cmd_buf,
  //   {
  //     {etna::Binding{0, generatedTex.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral)}}
  //   });

  // vk::DescriptorSet vkSet = set.getVkSet();
  // current_cmd_buf.bindDescriptorSets(
  //   vk::PipelineBindPoint::eGraphics,
  //   toyPipeline.getVkPipelineLayout(),
  //   0,
  //   1,
  //   &vkSet,
  //   0,
  //   nullptr);
  pushUniformConstants(current_cmd_buf, genPipeline, vk::ShaderStageFlagBits::eFragment);
  current_cmd_buf.draw(3, 1, 0, 0);
  etna::set_state(
    current_cmd_buf,
    generatedTex.get(),
    vk::PipelineStageFlagBits2::eFragmentShader,
    vk::AccessFlagBits2::eShaderRead,
    vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::ImageAspectFlagBits::eColor);
}

void App::prepareToy(vk::CommandBuffer& current_cmd_buf, vk::Image& backbuffer, vk::ImageView& backbuffer_view)
{
  auto toyComputeInfo = etna::get_shader_program("toy");
  etna::set_state(
    current_cmd_buf,
    backbuffer,
    vk::PipelineStageFlagBits2::eFragmentShader,
    {vk::AccessFlagBits2::eShaderRead},
    vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::ImageAspectFlagBits::eColor
  );

  etna::RenderTargetState state{
    current_cmd_buf,
    {{}, {resolution.x, resolution.y}},
    {{backbuffer, backbuffer_view}},
    {}
  };

  current_cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, toyPipeline.getVkPipeline());

  auto set = etna::create_descriptor_set(
    toyComputeInfo.getDescriptorLayoutId(0),
    current_cmd_buf,
    {
      etna::Binding{0, generatedTex.genBinding(defaultSampler.get(), vk::ImageLayout::eReadOnlyOptimal)},
    });

  vk::DescriptorSet vkSet = set.getVkSet();
  current_cmd_buf.bindDescriptorSets(
    vk::PipelineBindPoint::eGraphics,
    toyPipeline.getVkPipelineLayout(),
    0,
    1,
    &vkSet,
    0,
    nullptr);

  pushUniformConstants(current_cmd_buf, toyPipeline, vk::ShaderStageFlagBits::eFragment);
  
  current_cmd_buf.draw(3, 1, 0, 0);
}

void App::drawFrame()
{
  // First, get a command buffer to write GPU commands into.
  auto currentCmdBuf = commandManager->acquireNext();

  // Next, tell Etna that we are going to start processing the next frame.
  etna::begin_frame();

  // And now get the image we should be rendering the picture into.
  auto nextSwapchainImage = vkWindow->acquireNext();

  // When window is minimized, we can't render anything in Windows
  // because it kills the swapchain, so we skip frames in this case.
  if (nextSwapchainImage)
  {
    auto [backbuffer, backbufferView, backbufferAvailableSem] = *nextSwapchainImage;

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      // TODO: Record your commands here!
      auto genTexImg = generatedTex.get();
      auto genTexImgView = generatedTex.getView({});
      prepareGen(currentCmdBuf, genTexImg, genTexImgView);
      prepareToy(currentCmdBuf, backbuffer, backbufferView);

      // At the end of "rendering", we are required to change how the pixels of the
      // swpchain image are laid out in memory to something that is appropriate
      // for presenting to the window (while preserving the content of the pixels!).
      etna::set_state(
        currentCmdBuf,
        backbuffer,
        // This looks weird, but is correct. Ask about it later.
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        {},
        vk::ImageLayout::ePresentSrcKHR,
        vk::ImageAspectFlagBits::eColor);
      // And of course flush the layout transition.
      etna::flush_barriers(currentCmdBuf);
    }
    ETNA_CHECK_VK_RESULT(currentCmdBuf.end());

    // We are done recording GPU commands now and we can send them to be executed by the GPU.
    // Note that the GPU won't start executing our commands before the semaphore is
    // signalled, which will happen when the OS says that the next swapchain image is ready.
    auto renderingDone =
      commandManager->submit(std::move(currentCmdBuf), std::move(backbufferAvailableSem));

    // Finally, present the backbuffer the screen, but only after the GPU tells the OS
    // that it is done executing the command buffer via the renderingDone semaphore.
    const bool presented = vkWindow->present(std::move(renderingDone), backbufferView);

    if (!presented)
      nextSwapchainImage = std::nullopt;
  }

  etna::end_frame();

  // After a window us un-minimized, we need to restore the swapchain to continue rendering.
  if (!nextSwapchainImage && osWindow->getResolution() != glm::uvec2{0, 0})
  {
    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });
    ETNA_VERIFY((resolution == glm::uvec2{w, h}));
  }
}
