#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include "engine.hpp"
#include <string.h>
#include <cstdint>   // Necessary for uint32_t
#include <limits>    // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp
#include <set>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
static bool isDebugBuild           = true;
static bool enableValidationLayers = true;

const std::vector<const char*> validationLayers = {
  "VK_LAYER_KHRONOS_validation"};

const std::vector<const char*> deviceExtensions = {};

namespace NextVideo {
static const char* engineName      = "No engine";
static const char* applicationName = "Test application";

struct VKRenderer : public IRenderer {

  /* VK CALLBACKS */

  static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    dprintf(2, "validation layer: %s\n", pCallbackData->pMessage);
    return VK_FALSE;
  }

  /** VK Validation layers */

  bool validationLayersSupport(const std::vector<const char*>& requested) {

    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    for (const char* layerName : requested) {
      bool layerFound = false;
      for (const auto& layerProperties : availableLayers) {
        if (strcmp(layerName, layerProperties.layerName) == 0) {
          layerFound = true;
          break;
        }
      }
      if (!layerFound) {
        return false;
      }
    }
    return true;
  }
  /** VK Extensions */

  std::vector<const char*> getRequiredExtensions() {
    SurfaceExtensions        extensions = _desc.surface->getExtensions();
    std::vector<const char*> result;
    for (int i = 0; i < extensions.count; i++) result.push_back(extensions.names[i]);

    if (enableValidationLayers) {
      result.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return result;
  }

  /** VK INSTANCE **/
  void instanceCreate() {
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = applicationName;
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = engineName;
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_0;

    VkInstanceCreateInfo pCreateInfo{};
    pCreateInfo.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    pCreateInfo.pApplicationInfo = &appInfo;

    auto extensions                     = getRequiredExtensions();
    pCreateInfo.enabledExtensionCount   = extensions.size();
    pCreateInfo.ppEnabledExtensionNames = extensions.data();
    pCreateInfo.enabledLayerCount       = 0;

    if (isDebugBuild) {
      for (int i = 0; i < extensions.size(); i++) {
        LOG("[VK] Asking for extension %d %s\n", i, extensions[i]);
      }
    }
    if (enableValidationLayers) {
      HARD_CHECK(validationLayersSupport(validationLayers), "Validation layers requested but not supported\n");
      pCreateInfo.ppEnabledLayerNames = validationLayers.data();
      pCreateInfo.enabledLayerCount   = validationLayers.size();
      LOG("[VK] Validation layers requested\n");
    }

    HARD_CHECK(vkCreateInstance(&pCreateInfo, mAllocator, &mInstance) == VK_SUCCESS, "Error creating instance");
    LOG("[VK] Instance created\n");
  }

  void instanceDestroy() {
    vkDestroyInstance(mInstance, mAllocator);
  }

  /** VK INSTANCE END */


  /** VK Messenger */
  void setupMessenger() {
  }

  /** VK DEVICES*/

  std::vector<VkPhysicalDevice> getDevices() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());
    return devices;
  }

  bool deviceIsDiscrete(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures   deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
  }

  int deviceRate(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures   deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    int score = 0;
    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
    score += deviceProperties.limits.maxImageDimension2D;
    return score;
  }

  void deviceLog(VkPhysicalDevice device) {

    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures   deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    LOG("[VK] Device %s vendor %d\n", deviceProperties.deviceName, deviceProperties.vendorID);
  }

  VkPhysicalDevice pickPhysicalDevice() {
    auto devices = getDevices();
    HARD_CHECK(devices.size() > 0, "[VK] No devices\n");
    std::vector<int> scores(devices.size());
    std::vector<int> indices(devices.size());

    int maxScore = 0;
    for (int i = 0; i < scores.size(); i++) {
      scores[i] = deviceRate(devices[i]);
      indices.push_back(i);
      maxScore = std::max(scores[i], maxScore);
    }

    std::sort(indices.begin(), indices.end(), [&](int lhs, int rhs) { return scores[lhs] < scores[rhs]; });

    LOG("[VK] Choosing device on score based criteria (%lu candidates)\n", scores.size());
    LOG("[VK] Device %d score %d (maxScore %d)\n", indices[0], scores[indices[0]], maxScore);
    deviceLog(devices[indices[0]]);

    return devices[indices[0]];
  }

  /* Queue famililies */

  struct QueueFamilyIndices {
    bool     hasGraphicsFamily = false;
    bool     hasComputeFamily  = false;
    bool     hasTransferFamily = false;
    bool     hasPresentFamily  = false;
    uint32_t graphicsFamily;
    uint32_t computeFamily;
    uint32_t transferFamily;
    uint32_t presentFamily;
  };

  QueueFamilyIndices physicalDeviceQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;
    uint32_t           queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
    int i = 0;
    for (const auto& queueFamily : queueFamilies) {

      bool graphics;
      bool compute;
      bool transfer;
      bool present;
      if (graphics = queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        indices.graphicsFamily    = i;
        indices.hasGraphicsFamily = true;
      }
      if (compute = queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
        indices.computeFamily    = i;
        indices.hasComputeFamily = true;
      }

      if (transfer = queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
        indices.transferFamily    = i;
        indices.hasTransferFamily = true;
      }
      if (mMainSurface->mSurface) {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, mMainSurface->mSurface, &presentSupport);
        if (presentSupport) {
          indices.hasPresentFamily = true;
          indices.presentFamily    = i;
        }

        present = presentSupport;
      } else {
        present = false;
      }

      if (isDebugBuild) {
        LOG("#%d0 \tGraphics %d \tPresent %d \tCompute %d \tTransfer %d\n", i, graphics, present, compute, transfer);
      }

      i++;
    }

    return indices;
  }

  bool physicalDeviceCheckExtensionSupport(VkPhysicalDevice device, std::vector<const char*>& extensions) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(extensions.begin(), extensions.end());

    for (const auto& extension : availableExtensions) {
      requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
  }

  struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR        capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;
  };

  SwapChainSupportDetails physicalDeviceSwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    LOG("[VK] Querying swapchain support");
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0) {
      details.formats.resize(formatCount);
      vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
      details.presentModes.resize(presentModeCount);
      vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }
    return details;
  }

  struct SwapChain {
    VkSurfaceFormatKHR surfaceFormat;
    VkPresentModeKHR   presentMode;
    VkExtent2D         extent;
    int                imageCount;
    VkSwapchainKHR     swapChain;
  };


  //TODO: Choose method
  VkSurfaceFormatKHR swapChainChooseFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {

    for (const auto& availableFormat : availableFormats) {
      if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        return availableFormat;
      }
    }
    return availableFormats[0];
  }

  //TODO: Choose method
  VkPresentModeKHR swapChainChoosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    return VK_PRESENT_MODE_FIFO_KHR;
  }

  VkExtent2D swapChainChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, ISurface* surface) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    } else {
      int width  = surface->getWidth();
      int height = surface->getHeight();


      VkExtent2D actualExtent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)};

      actualExtent.width  = glm::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
      actualExtent.height = glm::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

      return actualExtent;
    }
  }

  SwapChain swapChainCreate(SwapChainSupportDetails swapChainSupport, ISurface* surfaceWindow, VkSurfaceKHR surface, VkDevice device) {
    SwapChain swapChain;

    swapChain.surfaceFormat = swapChainChooseFormat(swapChainSupport.formats);
    swapChain.presentMode   = swapChainChoosePresentMode(swapChainSupport.presentModes);
    swapChain.extent        = swapChainChooseSwapExtent(swapChainSupport.capabilities, surfaceWindow);

    swapChain.imageCount = swapChainSupport.capabilities.minImageCount + 1;

    if (swapChainSupport.capabilities.maxImageCount > 0 && swapChain.imageCount > swapChainSupport.capabilities.maxImageCount) {
      swapChain.imageCount = swapChainSupport.capabilities.maxImageCount;
    }
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType   = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;

    createInfo.minImageCount    = swapChain.imageCount;
    createInfo.imageFormat      = swapChain.surfaceFormat.format;
    createInfo.imageColorSpace  = swapChain.surfaceFormat.colorSpace;
    createInfo.imageExtent      = swapChain.extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    createInfo.preTransform   = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    createInfo.presentMode  = swapChain.presentMode;
    createInfo.clipped      = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    HARD_CHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain.swapChain) == VK_SUCCESS, "Error creating swap chain\n");
    return swapChain;
  }

  struct DeviceDesc {
    int countGraphics = 1;
    int countCompute  = 1;
    int countTransfer = 1;

    std::vector<const char*> requestedValidationLayers;
  };


  struct Device {

    Device(DeviceDesc desc) { this->desc = desc; }

    ~Device() {
      vkDestroyDevice(device, nullptr);
      LOG("[VK] Device destroyed\n");
    }

    std::vector<VkQueue> graphicQueues;
    std::vector<VkQueue> transferQueues;
    std::vector<VkQueue> computeQueues;
    VkQueue              presentQueue;

    SwapChainSupportDetails swapChainSupport;
    SwapChain               swapChain;

    VkDevice    device;
    DeviceDesc  desc;
    VKRenderer* renderer;
  };

  Device* deviceCreate(VkPhysicalDevice physicalDevice, DeviceDesc desc) {

    Device* device   = new Device(desc);
    device->renderer = this;

    // QUEUES
    QueueFamilyIndices indices = physicalDeviceQueueFamilies(physicalDevice);

    VkDeviceQueueCreateInfo queueCreateInfos[3];

    int queueInfoBase = 0;
    int queueGraphics = -1;
    int queueCompute  = -1;
    int queueTransfer = -1;

    float queuePriority = 1.0f;

    if (indices.hasGraphicsFamily) {
      VkDeviceQueueCreateInfo queueCreateInfo{};
      queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfo.queueFamilyIndex = indices.graphicsFamily;
      queueCreateInfo.queueCount       = desc.countGraphics;
      queueCreateInfo.pQueuePriorities = &queuePriority;

      queueCreateInfos[queueInfoBase] = queueCreateInfo;
      queueGraphics                   = queueInfoBase++;

      LOG("[VK] Graphics found %d\n", queueGraphics);
      device->graphicQueues.resize(desc.countCompute);
    }

    if (indices.hasComputeFamily) {
      VkDeviceQueueCreateInfo queueCreateInfo{};
      queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfo.queueFamilyIndex = indices.computeFamily;
      queueCreateInfo.queueCount       = desc.countCompute;
      queueCreateInfo.pQueuePriorities = &queuePriority;

      queueCreateInfos[queueInfoBase] = queueCreateInfo;
      queueCompute                    = queueInfoBase++;
      LOG("[VK] Compute found %d\n", queueCompute);
      device->computeQueues.resize(desc.countCompute);
    }

    if (indices.hasTransferFamily) {
      VkDeviceQueueCreateInfo queueCreateInfo{};
      queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfo.queueFamilyIndex = indices.transferFamily;
      queueCreateInfo.queueCount       = desc.countTransfer;
      queueCreateInfo.pQueuePriorities = &queuePriority;

      queueCreateInfos[queueInfoBase] = queueCreateInfo;
      queueTransfer                   = queueInfoBase++;
      LOG("[VK] Transfer found %d\n", queueTransfer);
      device->transferQueues.resize(desc.countCompute);
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.pQueueCreateInfos    = queueCreateInfos;
    createInfo.queueCreateInfoCount = queueInfoBase;

    VERIFY(queueInfoBase > 0, "[VK] No queues supported on device\n");


    // FEATURES
    VkPhysicalDeviceFeatures deviceFeatures{};
    createInfo.pEnabledFeatures      = &deviceFeatures;
    createInfo.enabledExtensionCount = 0;

    if (enableValidationLayers) {
      createInfo.enabledLayerCount   = desc.requestedValidationLayers.size();
      createInfo.ppEnabledLayerNames = desc.requestedValidationLayers.data();
    } else {
      createInfo.enabledLayerCount = 0;
    }

    VkDevice deviceHandle;
    HARD_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &deviceHandle) == VK_SUCCESS, "[VK] Error creating device");

    if (indices.hasGraphicsFamily) {
      for (int i = 0; i < desc.countGraphics; i++) {
        vkGetDeviceQueue(deviceHandle, indices.graphicsFamily, i, &device->graphicQueues[i]);
      }
    }

    if (indices.hasTransferFamily) {
      for (int i = 0; i < desc.countTransfer; i++) {
        vkGetDeviceQueue(deviceHandle, indices.transferFamily, i, &device->transferQueues[i]);
      }
    }

    if (indices.hasComputeFamily) {
      for (int i = 0; i < desc.countCompute; i++) {
        vkGetDeviceQueue(deviceHandle, indices.computeFamily, i, &device->computeQueues[i]);
      }
    }

    if (indices.hasPresentFamily) {
      vkGetDeviceQueue(deviceHandle, indices.presentFamily, 0, &device->presentQueue);
    }

    LOG("[VK] Device created\n");
    device->device = deviceHandle;

    if (mMainSurface) {
      device->swapChainSupport = physicalDeviceSwapChainSupport(physicalDevice, mMainSurface->mSurface);
      device->swapChain        = swapChainCreate(device->swapChainSupport, _desc.surface, mMainSurface->mSurface, device->device);
    }

    return device;
  }

  /* Surface */

  struct Surface {

    Surface(VKRenderer* renderer) { this->renderer = renderer; }

    ~Surface() {
      vkDestroySurfaceKHR(renderer->mInstance, mSurface, nullptr);
      LOG("[VK] Surface destroyed\n");
    }

    VKRenderer*  renderer;
    VkSurfaceKHR mSurface;
  };

  struct GLFWSurface : public Surface {

    GLFWSurface(VKRenderer* renderer) :
      Surface(renderer) {
      HARD_CHECK(glfwCreateWindowSurface(renderer->mInstance, (GLFWwindow*)renderer->desc().surface->native(), nullptr, &mSurface) == VK_SUCCESS, "Error creating surface\n");
      LOG("[VK] Surface created\n");
    }
  };

  Surface* surfaceCreate() {
    return new GLFWSurface(this);
  }

  VKRenderer(RendererDesc desc) {
    this->_desc = desc;
    instanceCreate();
    setupMessenger();
    DeviceDesc device_desc;
    device_desc.requestedValidationLayers = validationLayers;

    mMainSurface = surfaceCreate();
    mMainDevice  = deviceCreate(pickPhysicalDevice(), device_desc);
  }

  ~VKRenderer() {
    delete mMainDevice;
    delete mMainSurface;
    instanceDestroy();
  }

  virtual void render(Scene* scene) override {}
  virtual void upload(Scene* scene) override {}

  private:
  Surface*               mMainSurface = nullptr;
  Device*                mMainDevice  = nullptr;
  VkInstance             mInstance;
  VkAllocationCallbacks* mAllocator = nullptr;
};

ENGINE_API IRenderer* rendererCreate(RendererDesc desc) { return new VKRenderer(desc); }

ENGINE_API RendererBackendDefaults rendererDefaults() {
  static RendererBackendDefaults def = {.glfw_noApi = true};
  return def;
}
} // namespace NextVideo
