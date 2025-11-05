#include "../inc/const.h"
#include "../inc/def.h"
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <stdio.h>
#include <threads.h>
#include <vulkan/vulkan_core.h>
#define VKDBG 1

static SDL_Window *win;
static VkInstance inst;
static VkSurfaceKHR srf;
extern u32 w, h;
void crtwin() {
  SDL_Init(SDL_INIT_VIDEO) ||
      printf("failed to init sdl3: %s\n", SDL_GetError());
  auto displays = SDL_GetDisplays(0);
  auto modes = SDL_GetCurrentDisplayMode(displays[0]);
  f32 scale = modes->pixel_density;
  w = 1600, h = 1000;
#define WFLAGS                                                                 \
  SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE
  win = SDL_CreateWindow("test", w, h, WFLAGS);
  w *= scale, h *= scale;
  win || printf("failed to create window: %s\n", SDL_GetError());
}
#if VKDBG
static u32 dbgh(VkDebugUtilsMessageSeverityFlagBitsEXT s,
                VkDebugUtilsMessageTypeFlagsEXT t,
                const VkDebugUtilsMessengerCallbackDataEXT *d, void *) {
  printf("[severity: %x\ttype: %x] %s\n", s, t, d->pMessage);
  return 0;
}
#endif
void crtinst() {
  u32 cnt;
  const char *exts[8];
  const char *const *SDLexts = SDL_Vulkan_GetInstanceExtensions(&cnt);
  memcpy(exts, SDLexts, cnt * sizeof(usize));

  exts[cnt++] = "VK_EXT_swapchain_colorspace";
  VkApplicationInfo appinfo = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .apiVersion = VK_API_VERSION_1_4,
  };
  VkInstanceCreateInfo crtinfo = {.sType =
                                      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                  .pApplicationInfo = &appinfo,
                                  .enabledExtensionCount = cnt,
                                  .ppEnabledExtensionNames = exts};
#if VKDBG
  exts[cnt++] = "VK_EXT_debug_utils";
  crtinfo.enabledExtensionCount = cnt;
  crtinfo.enabledLayerCount = 1;
  crtinfo.ppEnabledLayerNames = &(const char *){"VK_LAYER_KHRONOS_validation"};
  VkDebugUtilsMessengerCreateInfoEXT debug_info = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = dbgh};
  crtinfo.pNext = &debug_info;
#endif
  auto instrslt = vkCreateInstance(&crtinfo, 0, &inst);
  instrslt &&printf("failed to create vulkan instance %d\n", instrslt);
}
void crtsrf() {
  SDL_Vulkan_CreateSurface(win, inst, 0, &srf) ||
      printf("failed to create surface: %s\n", SDL_GetError());
  ;
}
static VkPhysicalDevice selpdev(VkInstance inst) {
  u32 cnt = 1;
  VkPhysicalDevice dev;
  vkEnumeratePhysicalDevices(inst, &cnt, &dev);
  VkPhysicalDeviceProperties p;
  vkGetPhysicalDeviceProperties(dev, &p);
  printf("physical device 0:%s\n", p.deviceName);
  return dev;
}
static VkDevice crtdev(VkPhysicalDevice pdev) {
  f32 queue_p[] = {1};
  VkDeviceQueueCreateInfo qci[] = {
      {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
       .queueFamilyIndex = 0,
       .queueCount = 1,
       .pQueuePriorities = queue_p},
  };
  const char *dev_exts[] = {"VK_KHR_swapchain"};
  VkPhysicalDeviceDynamicRenderingFeatures dynfeat = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
      .dynamicRendering = 1};
  VkDeviceCreateInfo dev_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = &dynfeat,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = qci,
      .enabledExtensionCount = 1,
      .ppEnabledExtensionNames = dev_exts,
  };
  VkDevice dev;
  VkResult devrslt = vkCreateDevice(pdev, &dev_info, 0, &dev);
  devrslt &&printf("failed to create device: %d\n", devrslt);
  return dev;
}
const VkFormat colorfmt = VK_FORMAT_R16G16B16A16_SFLOAT;
const auto colorspace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
#define crtshdinfo(src, stg)                                                   \
  extern const char bin(src, start)[], bin(src, end)[];                        \
  shdinfo.codeSize = bin(src, end) - bin(src, start);                          \
  shdinfo.pCode = (u32 *)bin(src, start);                                      \
  VkShaderModule src##_shdmod;                                                 \
  vkCreateShaderModule(dev, &shdinfo, 0, &src##_shdmod);                       \
  VkPipelineShaderStageCreateInfo src##_info = {                               \
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,            \
      .stage = VK_SHADER_STAGE_##stg##_BIT,                                    \
      .module = src##_shdmod,                                                  \
      .pName = "main"}

static VkDeviceMemory memalloc(VkDevice dev, VkMemoryRequirements memreq,
                               VkPhysicalDeviceMemoryProperties memprop,
                               usize flags) {
  for (usize i = 0; i < memprop.memoryTypeCount; ++i)
    if (memreq.memoryTypeBits & (1 << i) &&
        memprop.memoryTypes[i].propertyFlags == flags) {
      VkMemoryAllocateInfo allocinfo = {
          .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
          .allocationSize = memreq.size,
          .memoryTypeIndex = i,
      };
      VkDeviceMemory devmem;
      vkAllocateMemory(dev, &allocinfo, 0, &devmem);
      return devmem;
    }
  printf("failed to find memory type: %x\n", memreq.memoryTypeBits);
  return 0;
}
static void genfont(u8 *p) {
  constexpr char ascii[] = {
#embed "../ascii"
  };
  for (usize i = 0; i < 1520; ++i)
    for (usize j = 0; j < 8; ++j)
      p[i * 8 + j] = (char)(ascii[i] << j) >> 7;
}
static f32 display(vec4 *ui, usize *cnt, f32 x, f32 y, f32 w, char *buf) {
  usize c;
  while ((c = *buf++)) {
    ui[(*cnt)++] = (vec4){x += w, y, c * 16 - 512};
  }
  return x;
}
int gpu(void *) {
  auto pdev = selpdev(inst);
  auto dev = crtdev(pdev);
  VkQueue devque;
  vkGetDeviceQueue(dev, 0, 0, &devque);
  VkPhysicalDeviceMemoryProperties memprop;
  vkGetPhysicalDeviceMemoryProperties(pdev, &memprop);

  VkCommandPoolCreateInfo poolinfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .queueFamilyIndex = 0,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
               VK_COMMAND_POOL_CREATE_TRANSIENT_BIT};
  VkCommandPool cmdpool;
  vkCreateCommandPool(dev, &poolinfo, 0, &cmdpool);

  VkCommandBufferAllocateInfo cmdallocinfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = cmdpool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1};
  VkCommandBuffer cmdbuf;
  vkAllocateCommandBuffers(dev, &cmdallocinfo, &cmdbuf);

  VkExtent3D asciiext = {8, 2048, 1};
  VkImageCreateInfo imginfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .extent = asciiext,
      .mipLevels = 1,
      .arrayLayers = 1,
      .format = VK_FORMAT_R8_UNORM,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .samples = 1,
  };
  VkImage asciimg;
  vkCreateImage(dev, &imginfo, 0, &asciimg);
  VkMemoryRequirements memreq;
  vkGetImageMemoryRequirements(dev, asciimg, &memreq);
  auto asciimem = memalloc(dev, memreq, memprop, 1);
  vkBindImageMemory(dev, asciimg, asciimem, 0);

  VkImageSubresourceRange subrscrange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

  VkBufferCreateInfo bufinfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = memreq.size,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };
  VkBuffer asciibuf, instbuf, pbuf;
  vkCreateBuffer(dev, &bufinfo, 0, &asciibuf);
  bufinfo.size = 65536;
  bufinfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  vkCreateBuffer(dev, &bufinfo, 0, &instbuf);
  vkCreateBuffer(dev, &bufinfo, 0, &pbuf);

  vkGetBufferMemoryRequirements(dev, asciibuf, &memreq);
  auto bufmem = memalloc(dev, memreq, memprop, 6);
  vkBindBufferMemory(dev, asciibuf, bufmem, 0);
  u8 *asciidata;
  vkMapMemory(dev, bufmem, 0, memreq.size, 0, (void *)&asciidata);
  genfont(asciidata);

  vkGetBufferMemoryRequirements(dev, instbuf, &memreq);
  auto uimem = memalloc(dev, memreq, memprop, 7);
  vkBindBufferMemory(dev, instbuf, uimem, 0);
  vec4 *uidata;
  vkMapMemory(dev, uimem, 0, memreq.size, 0, (void *)&uidata);

  vkGetBufferMemoryRequirements(dev, pbuf, &memreq);
  auto pmem = memalloc(dev, memreq, memprop, 7);
  vkBindBufferMemory(dev, pbuf, pmem, 0);
  extern u8 *pdata;
  vkMapMemory(dev, pmem, 0, memreq.size, 0, (void *)&pdata);

  VkFence fence;
  VkFenceCreateInfo fenceInfo = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  vkCreateFence(dev, &fenceInfo, 0, &fence);
  VkCommandBufferBeginInfo beginfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};

  VkBufferImageCopy region = {
      .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
      .imageExtent = asciiext,
  };
  VkImageMemoryBarrier imgbarr = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .subresourceRange = subrscrange,
      .image = asciimg,
  };

  vkBeginCommandBuffer(cmdbuf, &beginfo);
  imgbarr.srcAccessMask = 0;
  imgbarr.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  imgbarr.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imgbarr.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &imgbarr);
  vkCmdCopyBufferToImage(cmdbuf, asciibuf, asciimg,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  imgbarr.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  imgbarr.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  imgbarr.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  imgbarr.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                       VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &imgbarr);
  vkEndCommandBuffer(cmdbuf);
  VkPipelineStageFlags waitstag = 0;
  VkSubmitInfo submitinfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pWaitDstStageMask = &waitstag,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmdbuf,
  };
  vkQueueSubmit(devque, 1, &submitinfo, fence);
  waitstag = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  submitinfo.waitSemaphoreCount = 1;
  submitinfo.signalSemaphoreCount = 1;

  VkSamplerCreateInfo sampinfo = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_NEAREST,
      .minFilter = VK_FILTER_NEAREST,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .unnormalizedCoordinates = 1,
  };
  VkSampler samp;
  vkCreateSampler(dev, &sampinfo, 0, &samp);

  VkDescriptorSetLayoutBinding desclytbind[] = {
      {
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
  };
  VkDescriptorSetLayoutCreateInfo desclytinfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = desclytbind,
  };
  VkDescriptorSetLayout desclyt;
  vkCreateDescriptorSetLayout(dev, &desclytinfo, 0, &desclyt);

  VkDescriptorPoolSize descpoolsize[] = {
      {
          .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
      },
  };
  VkDescriptorPoolCreateInfo descpoolinfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .poolSizeCount = 1,
      .pPoolSizes = descpoolsize,
      .maxSets = 1,
  };
  VkDescriptorPool descpool;
  vkCreateDescriptorPool(dev, &descpoolinfo, 0, &descpool);

  VkDescriptorSetAllocateInfo descallocinfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = descpool,
      .descriptorSetCount = 1,
      .pSetLayouts = &desclyt,
  };
  VkDescriptorSet descset;
  vkAllocateDescriptorSets(dev, &descallocinfo, &descset);

  VkImageViewCreateInfo imgviewinfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .image = asciimg,
      .format = VK_FORMAT_R8_UNORM,
      .subresourceRange = subrscrange,
  };
  VkImageView asciimgview;
  vkCreateImageView(dev, &imgviewinfo, 0, &asciimgview);
  VkDescriptorImageInfo descimginfo = {
      .imageView = asciimgview,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .sampler = samp,
  };
  VkWriteDescriptorSet wrdesc = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descset,
      .dstBinding = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImageInfo = &descimginfo,
  };
  vkUpdateDescriptorSets(dev, 1, &wrdesc, 0, 0);

  VkPushConstantRange cstrg = {
      .stageFlags = VK_SHADER_STAGE_ALL,
      .offset = 0,
      .size = 24,
  };
  VkPipelineLayoutCreateInfo lytinfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pSetLayouts = &desclyt,
      .setLayoutCount = 1,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &cstrg,
  };
  VkPipelineLayout layout;
  vkCreatePipelineLayout(dev, &lytinfo, 0, &layout);

  VkShaderModuleCreateInfo shdinfo = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
  };
  VkVertexInputBindingDescription vertin = {0, 1, VK_VERTEX_INPUT_RATE_VERTEX};
  VkVertexInputAttributeDescription vertdesc = {0, 0, VK_FORMAT_R8_UNORM, 0};
  VkPipelineVertexInputStateCreateInfo vertinfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &vertin,
      .vertexAttributeDescriptionCount = 1,
      .pVertexAttributeDescriptions = &vertdesc,
  };
  VkPipelineInputAssemblyStateCreateInfo asminfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
  };
  VkViewport viewport = {0, 0, w, h, 0, 1};
  VkRect2D scissor = {{0, 0}, {w, h}};
  VkPipelineViewportStateCreateInfo viewinfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1,
      .pViewports = &viewport,
      .pScissors = &scissor,
  };
  VkPipelineRasterizationStateCreateInfo rastinfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .lineWidth = 1,
  };
  VkPipelineMultisampleStateCreateInfo msinfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = 1,
  };
  VkPipelineDepthStencilStateCreateInfo depinfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
  };
  VkPipelineColorBlendAttachmentState attch = {
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = 15,
  };
  VkPipelineColorBlendStateCreateInfo colinfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &attch,
  };
  VkPipelineRenderingCreateInfo rendcinfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &colorfmt,
  };
  VkGraphicsPipelineCreateInfo grapinfo = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &rendcinfo,
      .stageCount = 2,
      .pVertexInputState = &vertinfo,
      .pInputAssemblyState = &asminfo,
      .pViewportState = &viewinfo,
      .pRasterizationState = &rastinfo,
      .pMultisampleState = &msinfo,
      .pDepthStencilState = &depinfo,
      .pColorBlendState = &colinfo,
      .layout = layout,
  };

  crtshdinfo(point_vert, VERTEX);
  crtshdinfo(point_frag, FRAGMENT);
  VkPipelineShaderStageCreateInfo pointinfo[] = {point_vert_info, point_frag_info};
  grapinfo.pStages = pointinfo;
  VkPipeline pointpipe;
  vkCreateGraphicsPipelines(dev, 0, 1, &grapinfo, 0, &pointpipe);

  crtshdinfo(line_vert, VERTEX);
  crtshdinfo(line_frag, FRAGMENT);
  VkPipelineShaderStageCreateInfo lineinfo[] = {line_vert_info, line_frag_info};
  grapinfo.pStages = lineinfo;
  vertin.stride = 32;
  vertin.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
  vertdesc.format = VK_FORMAT_R32G32B32A32_SFLOAT;
  asminfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
  grapinfo.pStages = lineinfo;
  VkPipeline linepipe;
  vkCreateGraphicsPipelines(dev, 0, 1, &grapinfo, 0, &linepipe);



  crtshdinfo(ui_vert, VERTEX);
  crtshdinfo(ui_frag, FRAGMENT);
  VkPipelineShaderStageCreateInfo uinfo[] = {ui_vert_info, ui_frag_info};
  vertin.stride = 16;
  vertin.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
  vertdesc.format = VK_FORMAT_R32G32B32A32_SFLOAT;
  attch.blendEnable = 1;
  asminfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
  grapinfo.pStages = uinfo;
  VkPipeline uipipe;
  vkCreateGraphicsPipelines(dev, 0, 1, &grapinfo, 0, &uipipe);

  VkSwapchainCreateInfoKHR swpchninfo = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .pNext = 0,
      .surface = srf,
      .minImageCount = 2,
      .imageFormat = colorfmt,
      .imageColorSpace = colorspace,
      .imageExtent = {w, h},
      .imageArrayLayers = 1,
      .imageUsage = 0x13,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .preTransform = 1,
      .compositeAlpha = 1,
      .presentMode = 0,
      .clipped = 1,
  };

  VkSwapchainKHR swpchn;
  vkCreateSwapchainKHR(dev, &swpchninfo, 0, &swpchn);
  u32 cnt = 2;
  VkImage imgs[2];
  vkGetSwapchainImagesKHR(dev, swpchn, &cnt, imgs);
  VkImageView imgview[2];
  imgviewinfo.format = colorfmt;
  for (usize i = 0; i < cnt; ++i) {
    imgviewinfo.image = imgs[i];
    vkCreateImageView(dev, &imgviewinfo, 0, imgview + i);
  }

  VkSemaphore imgsem, presem;
  VkSemaphoreCreateInfo seminfo = {.sType =
                                       VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  vkCreateSemaphore(dev, &seminfo, 0, &imgsem);
  vkCreateSemaphore(dev, &seminfo, 0, &presem);

  VkRenderingAttachmentInfo colatt = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = {},
  };
  VkRenderingInfo rendinfo = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea = {{0, 0}, {w, h}},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colatt,
  };
  VkPresentInfoKHR prsinfo = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .swapchainCount = 1,
      .pSwapchains = &swpchn,
  };

  struct {
    vec2 scl;
    u32 cnt;
  } cmn;
  extern bool quit;
  extern vec2 mousepos;
  u32 idx;
  time_t t0 = 0, fc = 0, fps;
  while (!quit) {
    ++fc;
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    if (ts.tv_sec != t0) {
      fps = fc;
      fc = 0;
      t0 = ts.tv_sec;
    }
    cmn.scl = 4 / (vec2){w, h};

    f32 fontw = cmn.scl.x * 8, fonty = 1 - cmn.scl.y * 16;
    char buf[32];
    usize uicnt = 1;
    sprintf(buf, "fps:%lu", fps);
    display(uidata, &uicnt, -1, fonty, fontw, buf);

    vkWaitForFences(dev, 1, &fence, 1, -1);
    vkResetFences(dev, 1, &fence);
    int rslt;
    while ((rslt = vkAcquireNextImageKHR(dev, swpchn, -1, imgsem, 0, &idx))) {
      if (rslt != VK_ERROR_OUT_OF_DATE_KHR && rslt != VK_SUBOPTIMAL_KHR) {
        printf("swapchain error:%d\n", rslt);
        quit = 1;
        break;
      }
      vkQueueWaitIdle(devque);
      for (usize i = 0; i < cnt; ++i)
        vkDestroyImageView(dev, imgview[i], 0);
      auto old = swpchn;
      swpchninfo.oldSwapchain = old,
      vkCreateSwapchainKHR(dev, &swpchninfo, 0, &swpchn);
      vkDestroySwapchainKHR(dev, old, 0);
      vkGetSwapchainImagesKHR(dev, swpchn, &cnt, imgs);
      for (usize i = 0; i < cnt; ++i) {
        imgviewinfo.image = imgs[i];
        vkCreateImageView(dev, &imgviewinfo, 0, imgview + i);
      }
    }
    colatt.imageView = imgview[idx];
    imgbarr.image = imgs[idx],

    vkResetCommandBuffer(cmdbuf, 0);
    vkBeginCommandBuffer(cmdbuf, &beginfo);
    vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0,
                            1, &descset, 0, 0);
    vkCmdPushConstants(cmdbuf, layout, VK_SHADER_STAGE_ALL, 0, 20, &cmn);

    imgbarr.srcAccessMask = 0,
    imgbarr.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    imgbarr.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    imgbarr.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         0, 0, 0, 0, 0, 1, &imgbarr);

    vkCmdBeginRendering(cmdbuf, &rendinfo);

    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pointpipe);
    vkCmdBindVertexBuffers(cmdbuf, 0, 1, &pbuf, &(usize){0});
    vkCmdDraw(cmdbuf, 65536, 1, 0, 0);

    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, uipipe);
    vkCmdBindVertexBuffers(cmdbuf, 0, 1, &instbuf, &(usize){0});
    vkCmdDraw(cmdbuf, 4, uicnt, 0, 0);

    vkCmdEndRendering(cmdbuf);

    imgbarr.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    imgbarr.dstAccessMask = 0,
    imgbarr.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    imgbarr.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    vkCmdPipelineBarrier(cmdbuf,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, 1,
                         &imgbarr);
    vkEndCommandBuffer(cmdbuf);

    submitinfo.pWaitSemaphores = &imgsem;
    submitinfo.pSignalSemaphores = &presem;
    vkQueueSubmit(devque, 1, &submitinfo, fence);

    prsinfo.pWaitSemaphores = &presem;
    prsinfo.pImageIndices = &idx;
    vkQueuePresentKHR(devque, &prsinfo);
  }
  return 0;
}
