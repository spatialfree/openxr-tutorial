#include <DebugOutput.h>
#include <GraphicsAPI_OpenGL.h>
#include <OpenXRDebugUtils.h>

class OpenXRTutorial {
public:
  OpenXRTutorial(GraphicsAPI_Type apiType)
    : m_apiType(apiType) {
    if(!CheckGraphicsAPI_TypeIsValidForPlatform(m_apiType)) {
      std::cout << "ERROR: The provided Graphics API is not valid for this platform." << std::endl;
      DEBUG_BREAK;
    }
  }
  ~OpenXRTutorial() = default;

  void Run() {
    CreateInstance();
    CreateDebugMessenger();

    GetInstanceProperties();
    GetSystemID();

    GetViewConfigurationViews();
    GetEnvironmentBlendModes();

    CreateSession();
    CreateReferenceSpace();
    CreateSwapchain();

    while (m_applicationRunning) {
      PollSystemEvents();
      PollEvents();
      if (m_sessionRunning) {
        // Draw Frame.
        RenderFrame();
      }
    }

    DestroySwapchain();
    DestroyReferenceSpace();
    DestroySession();

    DestroyDebugMessenger();
    DestroyInstance();
  }

private:
  void CreateInstance() {
    XrApplicationInfo AI;
    strncpy(AI.applicationName, "OpenXR Tutorial Chapter 2", XR_MAX_APPLICATION_NAME_SIZE);
    AI.applicationVersion = 1;
    strncpy(AI.engineName, "OpenXR Engine", XR_MAX_ENGINE_NAME_SIZE);
    AI.engineVersion = 1;
    AI.apiVersion = XR_CURRENT_API_VERSION;

    m_instanceExtensions.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
    // TODO make sure this is already defined when we add this line.
    m_instanceExtensions.push_back(GetGraphicsAPIInstanceExtensionString(m_apiType));

    // Get all the API Layers from the OpenXR runtime.
    uint32_t apiLayerCount = 0;
    std::vector<XrApiLayerProperties> apiLayerProperties;
    OPENXR_CHECK(xrEnumerateApiLayerProperties(0, &apiLayerCount, nullptr), "Failed to enumerate ApiLayerProperties.");
    apiLayerProperties.resize(apiLayerCount, {XR_TYPE_API_LAYER_PROPERTIES});
    OPENXR_CHECK(xrEnumerateApiLayerProperties(apiLayerCount, &apiLayerCount, apiLayerProperties.data()), "Failed to enumerate ApiLayerProperties.");

    // Check the requested API layers against the ones from the OpenXR. If found add it to the Active API Layers.
    for (auto &requestLayer : m_apiLayers) {
      for (auto &layerProperty : apiLayerProperties) {
        // strcmp returns 0 if the strings match.
        if (strcmp(requestLayer.c_str(), layerProperty.layerName) != 0) {
          continue;
        } else {
          m_activeAPILayers.push_back(requestLayer.c_str());
          break;
        }
      }
    }

    // Get all the Instance Extensions from the OpenXR instance.
    uint32_t extensionCount = 0;
    std::vector<XrExtensionProperties> extensionProperties;
    OPENXR_CHECK(xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr), "Failed to enumerate InstanceExtensionProperties.");
    extensionProperties.resize(extensionCount, {XR_TYPE_EXTENSION_PROPERTIES});
    OPENXR_CHECK(xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, extensionProperties.data()), "Failed to enumerate InstanceExtensionProperties.");

    // Check the requested Instance Extensions against the ones from the OpenXR runtime.
    // If an extension is found add it to Active Instance Extensions.
    // Log error if the Instance Extension is not found.
    for (auto &requestedInstanceExtension : m_instanceExtensions) {
      bool found = false;
      for (auto &extensionProperty : extensionProperties) {
        // strcmp returns 0 if the strings match.
        if (strcmp(requestedInstanceExtension.c_str(), extensionProperty.extensionName) != 0) {
          continue;
        } else {
          m_activeInstanceExtensions.push_back(requestedInstanceExtension.c_str());
          found = true;
          break;
        }
      }
      if (!found) {
        std::cerr << "Failed to find OpenXR instance extension: " << requestedInstanceExtension << std::endl;
      }
    }

    XrInstanceCreateInfo instanceCI{XR_TYPE_INSTANCE_CREATE_INFO};
    instanceCI.createFlags = 0;
    instanceCI.applicationInfo = AI;
    instanceCI.enabledApiLayerCount = static_cast<uint32_t>(m_activeAPILayers.size());
    instanceCI.enabledApiLayerNames = m_activeAPILayers.data();
    instanceCI.enabledExtensionCount = static_cast<uint32_t>(m_activeInstanceExtensions.size());
    instanceCI.enabledExtensionNames = m_activeInstanceExtensions.data();
    OPENXR_CHECK(xrCreateInstance(&instanceCI, &m_xrInstance), "Failed to create Instance.");
  }
  void DestroyInstance() {
    OPENXR_CHECK(xrDestroyInstance(m_xrInstance), "Failed to destroy Instance.");
  }
  void CreateDebugMessenger() {
    // Check that "XR_EXT_debug_utils" is in the active Instance Extensions before creating an XrDebugUtilsMessengerEXT.
    if (IsStringInVector(m_activeInstanceExtensions, XR_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
      m_debugUtilsMessenger = CreateOpenXRDebugUtilsMessenger(m_xrInstance);  // From OpenXRDebugUtils.h.
    }
  }
  void DestroyDebugMessenger() {
    // Check that "XR_EXT_debug_utils" is in the active Instance Extensions before destroying the XrDebugUtilsMessengerEXT.
    if (m_debugUtilsMessenger != XR_NULL_HANDLE) {
      DestroyOpenXRDebugUtilsMessenger(m_xrInstance, m_debugUtilsMessenger);  // From OpenXRDebugUtils.h.
    }
  }
  void CreateSession() {
    XrSessionCreateInfo sessionCI{XR_TYPE_SESSION_CREATE_INFO};

    m_graphicsAPI = std::make_unique<GraphicsAPI_OpenGL>(m_xrInstance, m_systemID);

    sessionCI.next = m_graphicsAPI->GetGraphicsBinding();
    sessionCI.createFlags = 0;
    sessionCI.systemId = m_systemID;

    OPENXR_CHECK(xrCreateSession(m_xrInstance, &sessionCI, &m_session), "Failed to create Session.");
  }
  void DestroySession() {
    OPENXR_CHECK(xrDestroySession(m_session), "Failed to destroy Session.");
  }
  void PollEvents() {
    XrResult result = XR_SUCCESS;
    do {
      // Poll OpenXR for a new event.
      XrEventDataBuffer eventData{XR_TYPE_EVENT_DATA_BUFFER};
      result = xrPollEvent(m_xrInstance, &eventData);

      switch (eventData.type) {
      // Log the number of lost events from the runtime.
      case XR_TYPE_EVENT_DATA_EVENTS_LOST: {
        XrEventDataEventsLost *eventsLost = reinterpret_cast<XrEventDataEventsLost *>(&eventData);
        std::cout << "OPENXR: Events Lost: " << eventsLost->lostEventCount << std::endl;
        break;
      }
      // Log that an instance loss is pending and shutdown the application.
      case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
        XrEventDataInstanceLossPending *instanceLossPending = reinterpret_cast<XrEventDataInstanceLossPending *>(&eventData);
        std::cout << "OPENXR: Instance Loss Pending at: " << instanceLossPending->lossTime << std::endl;
        m_sessionRunning = false;
        m_applicationRunning = false;
        break;
      }
      // Log that the interaction profile has changed.
      case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED: {
        XrEventDataInteractionProfileChanged *interactionProfileChanged = reinterpret_cast<XrEventDataInteractionProfileChanged *>(&eventData);
        std::cout << "OPENXR: Interaction Profile changed for Session: " << interactionProfileChanged->session << std::endl;
        break;
      }
      // Log that there's a reference space change pending.
      // TODO: expand on this in text.
      case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
        XrEventDataReferenceSpaceChangePending *referenceSpaceChangePending = reinterpret_cast<XrEventDataReferenceSpaceChangePending *>(&eventData);
        std::cout << "OPENXR: Reference Space Change pending for Session: " << referenceSpaceChangePending->session << std::endl;
        break;
      }
      // Session State changes:
      case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
        XrEventDataSessionStateChanged *sessionStateChanged = reinterpret_cast<XrEventDataSessionStateChanged *>(&eventData);

        if (sessionStateChanged->state == XR_SESSION_STATE_READY) {
          // SessionState is ready. Begin the XrSession using the XrViewConfigurationType.
          XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
          sessionBeginInfo.primaryViewConfigurationType = m_viewConfiguration;
          OPENXR_CHECK(xrBeginSession(m_session, &sessionBeginInfo), "Failed to begin Session.");
          m_sessionRunning = true;
        }
        if (sessionStateChanged->state == XR_SESSION_STATE_STOPPING) {
          // SessionState is stopping. End the XrSession.
          OPENXR_CHECK(xrEndSession(m_session), "Failed to end Session.");
          m_sessionRunning = false;
        }
        if (sessionStateChanged->state == XR_SESSION_STATE_EXITING) {
          // SessionState is exiting. Exit the application.
          m_sessionRunning = false;
          m_applicationRunning = false;
        }
        if (sessionStateChanged->state == XR_SESSION_STATE_LOSS_PENDING) {
          // SessionState is loss pending. Exit the application.
          // It's possible to try a reestablish an XrInstance and XrSession, but we will simply exit here.
          m_sessionRunning = false;
          m_applicationRunning = false;
        }
        // Store state for reference across the appplication.
        m_sessionState = sessionStateChanged->state;
        break;
      }
      default: {
        break;
      }
      }

    } while (result == XR_SUCCESS);
  }
  void PollSystemEvents() {

  }
  void GetInstanceProperties() {
    XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
    OPENXR_CHECK(xrGetInstanceProperties(m_xrInstance, &instanceProperties), "Failed to get InstanceProperties.");

    std::cout << "OpenXR Runtime: " << instanceProperties.runtimeName << " - ";
    std::cout << XR_VERSION_MAJOR(instanceProperties.runtimeVersion) << ".";
    std::cout << XR_VERSION_MINOR(instanceProperties.runtimeVersion) << ".";
    std::cout << XR_VERSION_PATCH(instanceProperties.runtimeVersion) << std::endl;
  }
  void GetSystemID() {
    // Get the XrSystemId from the instance and the supplied XrFormFactor.
    XrSystemGetInfo systemGI{XR_TYPE_SYSTEM_GET_INFO};
    systemGI.formFactor = m_formFactor;
    OPENXR_CHECK(xrGetSystem(m_xrInstance, &systemGI, &m_systemID), "Failed to get SystemID.");

    // Get the System's properties for some general information about the hardware and the vendor.
    OPENXR_CHECK(xrGetSystemProperties(m_xrInstance, m_systemID, &m_systemProperties), "Failed to get SystemProperties.");
  }



  void GetViewConfigurationViews() {
    // Gets the View Configuration Views. The first call gets the size of the array that will be returned. The next call fills out the array.
    uint32_t viewConfigurationViewSize = 0;
    OPENXR_CHECK(xrEnumerateViewConfigurationViews(m_xrInstance, m_systemID, m_viewConfiguration, 0, &viewConfigurationViewSize, nullptr), "Failed to enumerate ViewConfiguration Views.");
    m_viewConfigurationViews.resize(viewConfigurationViewSize, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    OPENXR_CHECK(xrEnumerateViewConfigurationViews(m_xrInstance, m_systemID, m_viewConfiguration, viewConfigurationViewSize, &viewConfigurationViewSize, m_viewConfigurationViews.data()), "Failed to enumerate ViewConfiguration Views.");
  }
  void CreateSwapchain() {
    // Get the supported swapchain formats as an array of int64_t and ordered by runtime preference.
    uint32_t formatSize = 0;
    OPENXR_CHECK(xrEnumerateSwapchainFormats(m_session, 0, &formatSize, nullptr), "Failed to enumerate Swapchain Formats");
    std::vector<int64_t> formats(formatSize);
    OPENXR_CHECK(xrEnumerateSwapchainFormats(m_session, formatSize, &formatSize, formats.data()), "Failed to enumerate Swapchain Formats");

    const XrViewConfigurationView &viewConfigurationView = m_viewConfigurationViews[0];

    m_swapchainAndDepthImages.resize(m_viewConfigurationViews.size());
    for (SwapchainAndDepthImage &swapchainAndDepthImage : m_swapchainAndDepthImages) {
      // Fill out an XrSwapchainCreateInfo structure and create an XrSwapchain.
      XrSwapchainCreateInfo swapchainCI{XR_TYPE_SWAPCHAIN_CREATE_INFO};
      swapchainCI.createFlags = 0;
      swapchainCI.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
      swapchainCI.format = m_graphicsAPI->SelectColorSwapchainFormat(formats);          // Use GraphicsAPI to select the first compatible format.
      swapchainCI.sampleCount = viewConfigurationView.recommendedSwapchainSampleCount;  // Use the recommended values from the XrViewConfigurationView.
      swapchainCI.width = viewConfigurationView.recommendedImageRectWidth;
      swapchainCI.height = viewConfigurationView.recommendedImageRectHeight;
      swapchainCI.faceCount = 1;
      swapchainCI.arraySize = 1;
      swapchainCI.mipCount = 1;
      OPENXR_CHECK(xrCreateSwapchain(m_session, &swapchainCI, &swapchainAndDepthImage.swapchain), "Failed to create Swapchain");
      swapchainAndDepthImage.swapchainFormat = swapchainCI.format;  // Save the swapchain format for later use.

      // Get the number of images in the swapchain and allocate Swapchain image data via GraphicsAPI to store the returned array.
      uint32_t swapchainImageCount = 0;
      OPENXR_CHECK(xrEnumerateSwapchainImages(swapchainAndDepthImage.swapchain, 0, &swapchainImageCount, nullptr), "Failed to enumerate Swapchain Images.");
      XrSwapchainImageBaseHeader *swapchainImages = m_graphicsAPI->AllocateSwapchainImageData(swapchainAndDepthImage.swapchain, GraphicsAPI::SwapchainType::COLOR, swapchainImageCount);
      OPENXR_CHECK(xrEnumerateSwapchainImages(swapchainAndDepthImage.swapchain, swapchainImageCount, &swapchainImageCount, swapchainImages), "Failed to enumerate Swapchain Images.");

      // Fill out a GraphicsAPI::ImageCreateInfo structure and create a depth image.
      GraphicsAPI::ImageCreateInfo depthImageCI;
      depthImageCI.dimension = 2;
      depthImageCI.width = viewConfigurationView.recommendedImageRectWidth;
      depthImageCI.height = viewConfigurationView.recommendedImageRectHeight;
      depthImageCI.depth = 1;
      depthImageCI.mipLevels = 1;
      depthImageCI.arrayLayers = 1;
      depthImageCI.sampleCount = 1;
      depthImageCI.format = m_graphicsAPI->GetDepthFormat();
      depthImageCI.cubemap = false;
      depthImageCI.colorAttachment = false;
      depthImageCI.depthAttachment = true;
      depthImageCI.sampled = false;
      swapchainAndDepthImage.depthImage = m_graphicsAPI->CreateImage(depthImageCI);

      // Per image in the swapchain, fill out a GraphicsAPI::ImageViewCreateInfo structure and create a color image view.
      for (uint32_t i = 0; i < swapchainImageCount; i++) {
        GraphicsAPI::ImageViewCreateInfo imageViewCI;
        imageViewCI.image = m_graphicsAPI->GetSwapchainImage(swapchainAndDepthImage.swapchain, i);
        imageViewCI.type = GraphicsAPI::ImageViewCreateInfo::Type::RTV;
        imageViewCI.view = GraphicsAPI::ImageViewCreateInfo::View::TYPE_2D;
        imageViewCI.format = swapchainAndDepthImage.swapchainFormat;
        imageViewCI.aspect = GraphicsAPI::ImageViewCreateInfo::Aspect::COLOR_BIT;
        imageViewCI.baseMipLevel = 0;
        imageViewCI.levelCount = 1;
        imageViewCI.baseArrayLayer = 0;
        imageViewCI.layerCount = 1;
        swapchainAndDepthImage.colorImageViews.push_back(m_graphicsAPI->CreateImageView(imageViewCI));
      }

      // Fill out a GraphicsAPI::ImageViewCreateInfo structure and create a depth image view.
      GraphicsAPI::ImageViewCreateInfo imageViewCI;
      imageViewCI.image = swapchainAndDepthImage.depthImage;
      imageViewCI.type = GraphicsAPI::ImageViewCreateInfo::Type::DSV;
      imageViewCI.view = GraphicsAPI::ImageViewCreateInfo::View::TYPE_2D;
      imageViewCI.format = m_graphicsAPI->GetDepthFormat();
      imageViewCI.aspect = GraphicsAPI::ImageViewCreateInfo::Aspect::DEPTH_BIT;
      imageViewCI.baseMipLevel = 0;
      imageViewCI.levelCount = 1;
      imageViewCI.baseArrayLayer = 0;
      imageViewCI.layerCount = 1;
      swapchainAndDepthImage.depthImageView = m_graphicsAPI->CreateImageView(imageViewCI);
    }
  }
  void DestroySwapchain() {
    // Per view in the view configuration:
    for (SwapchainAndDepthImage &swapchainAndDepthImage : m_swapchainAndDepthImages) {
      // Destroy the color and depth image views from GraphicsAPI.
      m_graphicsAPI->DestroyImageView(swapchainAndDepthImage.depthImageView);
      for (void *&colorImageView : swapchainAndDepthImage.colorImageViews) {
        m_graphicsAPI->DestroyImageView(colorImageView);
      }

      // Destroy the depth image from GraphicsAPI
      m_graphicsAPI->DestroyImage(swapchainAndDepthImage.depthImage);

      // Free Swapchain Image Data
      m_graphicsAPI->FreeSwapchainImageData(swapchainAndDepthImage.swapchain);

      // Destory the swapchain.
      OPENXR_CHECK(xrDestroySwapchain(swapchainAndDepthImage.swapchain), "Failed to destroy Swapchain");
    }
  }
  void GetEnvironmentBlendModes() {
    // Retrieves the available blend modes. The first call gets the size of the array that will be returned. The next call fills out the array.
    uint32_t environmentBlendModeSize = 0;
    OPENXR_CHECK(xrEnumerateEnvironmentBlendModes(m_xrInstance, m_systemID, m_viewConfiguration, 0, &environmentBlendModeSize, nullptr), "Failed to enumerate EnvironmentBlend Modes.");
    m_environmentBlendModes.resize(environmentBlendModeSize);
    OPENXR_CHECK(xrEnumerateEnvironmentBlendModes(m_xrInstance, m_systemID, m_viewConfiguration, environmentBlendModeSize, &environmentBlendModeSize, m_environmentBlendModes.data()), "Failed to enumerate EnvironmentBlend Modes.");

    // Pick the first application supported blend mode supported by the hardware.
    for (const XrEnvironmentBlendMode &environmentBlendMode : m_applicationEnvironmentBlendModes) {
      if (std::find(m_environmentBlendModes.begin(), m_environmentBlendModes.end(), environmentBlendMode) != m_environmentBlendModes.end()) {
        m_environmentBlendMode = environmentBlendMode;
        break;
      }
    }
    if (m_environmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM) {
      std::cerr << "Failed to find a compatible blend mode. Defaulting to XR_ENVIRONMENT_BLEND_MODE_OPAQUE." << std::endl;
      m_environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    }
  }
  void CreateReferenceSpace() {
    // Fill out an XrReferenceSpaceCreateInfo structure and create a reference XrSpace, specifying a Local space with an identity pose as the origin.
    XrReferenceSpaceCreateInfo referenceSpaceCI{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    referenceSpaceCI.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    referenceSpaceCI.poseInReferenceSpace = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}};
    OPENXR_CHECK(xrCreateReferenceSpace(m_session, &referenceSpaceCI, &m_localOrStageSpace), "Failed to create ReferenceSpace.");
  }
  void DestroyReferenceSpace() {
    // Destroy the reference XrSpace.
    OPENXR_CHECK(xrDestroySpace(m_localOrStageSpace), "Failed to destroy Space.")
  }
  void RenderFrame() {
    // Get the XrFrameState for timing and rendering info.
    XrFrameState frameState{XR_TYPE_FRAME_STATE};
    XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
    OPENXR_CHECK(xrWaitFrame(m_session, &frameWaitInfo, &frameState), "Failed to wait for XR Frame.");

    // Tell the OpenXR compositor that the application is beginning the frame.
    XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    OPENXR_CHECK(xrBeginFrame(m_session, &frameBeginInfo), "Failed to begin the XR Frame.");

    // Variables for rendering and layer composition.
    bool rendered = false;
    std::vector<XrCompositionLayerBaseHeader *> layers;
    XrCompositionLayerProjection layerProjection{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    std::vector<XrCompositionLayerProjectionView> layerProjectionViews;

    // Check that the session is active and that we should render.
    bool sessionActive = (m_sessionState == XR_SESSION_STATE_SYNCHRONIZED || m_sessionState == XR_SESSION_STATE_VISIBLE || m_sessionState == XR_SESSION_STATE_FOCUSED);
    if (sessionActive && frameState.shouldRender) {
      // Render the stereo image and associate one of swapchain images with the XrCompositionLayerProjection structure.
      rendered = RenderLayer(frameState.predictedDisplayTime, layerProjection, layerProjectionViews);
      if (rendered) {
        layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader *>(&layerProjection));
      }
    }

    // Tell OpenXR that we are finished with this frame; specifying its display time, environment blending and layers.
    XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
    frameEndInfo.displayTime = frameState.predictedDisplayTime;
    frameEndInfo.environmentBlendMode = m_environmentBlendMode;
    frameEndInfo.layerCount = static_cast<uint32_t>(layers.size());
    frameEndInfo.layers = layers.data();
    OPENXR_CHECK(xrEndFrame(m_session, &frameEndInfo), "Failed to end the XR Frame.");
  }
  bool RenderLayer(const XrTime &predictedDisplayTime, XrCompositionLayerProjection &layerProjection, std::vector<XrCompositionLayerProjectionView> &layerProjectionViews) {

  }


private:
  XrInstance m_xrInstance = {};
  std::vector<const char *> m_activeAPILayers = {};
  std::vector<const char *> m_activeInstanceExtensions = {};
  std::vector<std::string> m_apiLayers = {};
  std::vector<std::string> m_instanceExtensions = {};

  XrDebugUtilsMessengerEXT m_debugUtilsMessenger = {};

  XrFormFactor m_formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  XrSystemId m_systemID = {};
  XrSystemProperties m_systemProperties = {XR_TYPE_SYSTEM_PROPERTIES};

  GraphicsAPI_Type m_apiType = UNKNOWN;
  std::unique_ptr<GraphicsAPI> m_graphicsAPI = nullptr;

  XrSession m_session = XR_NULL_HANDLE;
  XrSessionState m_sessionState = XR_SESSION_STATE_UNKNOWN;

  XrViewConfigurationType m_viewConfiguration = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

  bool m_applicationRunning = true;
  bool m_sessionRunning = false;



  std::vector<XrViewConfigurationView> m_viewConfigurationViews;

  struct SwapchainAndDepthImage {
    XrSwapchain swapchain = XR_NULL_HANDLE;
    int64_t swapchainFormat = 0;
    void *depthImage = nullptr;
    std::vector<void *> colorImageViews;
    void *depthImageView = nullptr;
  };
  std::vector<SwapchainAndDepthImage> m_swapchainAndDepthImages = {};

  std::vector<XrEnvironmentBlendMode> m_applicationEnvironmentBlendModes = {XR_ENVIRONMENT_BLEND_MODE_OPAQUE, XR_ENVIRONMENT_BLEND_MODE_ADDITIVE};
  std::vector<XrEnvironmentBlendMode> m_environmentBlendModes = {};
  XrEnvironmentBlendMode m_environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM;

  XrSpace m_localOrStageSpace = XR_NULL_HANDLE;
};

void OpenXRTutorial_Main(GraphicsAPI_Type apiType) {
  DebugOutput debugOutput;  // This redirects std::cerr and std::cout to the IDE's output or Android Studio's logcat.
  std::cout << "OpenXR Tutorial Chapter 2." << std::endl;

  OpenXRTutorial app(apiType);
  app.Run();
}

int main(int argc, char **argv) {
  OpenXRTutorial_Main(OPENGL);
}
