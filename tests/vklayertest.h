/*
 * Copyright (c) 2015-2019 The Khronos Group Inc.
 * Copyright (c) 2015-2019 Valve Corporation
 * Copyright (c) 2015-2019 LunarG, Inc.
 * Copyright (c) 2015-2019 Google, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Author: Chia-I Wu <olvaffe@gmail.com>
 * Author: Chris Forbes <chrisf@ijw.co.nz>
 * Author: Courtney Goeltzenleuchter <courtney@LunarG.com>
 * Author: Mark Lobodzinski <mark@lunarg.com>
 * Author: Mike Stroyan <mike@LunarG.com>
 * Author: Tobin Ehlis <tobine@google.com>
 * Author: Tony Barbour <tony@LunarG.com>
 * Author: Cody Northrop <cnorthrop@google.com>
 * Author: Dave Houlton <daveh@lunarg.com>
 * Author: Jeremy Kniager <jeremyk@lunarg.com>
 * Author: Shannon McPherson <shannon@lunarg.com>
 * Author: John Zulauf <jzulauf@lunarg.com>
 */

#ifndef VKLAYERTEST_H
#define VKLAYERTEST_H

#ifdef ANDROID
#include "vulkan_wrapper.h"
#else
#define NOMINMAX
#include <vulkan/vulkan.h>
#endif

#include "layers/vk_device_profile_api_layer.h"

#if defined(ANDROID)
#include <android/log.h>
#if defined(VALIDATION_APK)
#include <android_native_app_glue.h>
#endif
#endif

#include "icd-spv.h"
#include "test_common.h"
#include "vk_layer_config.h"
#include "vk_format_utils.h"
#include "vkrenderframework.h"
#include "vk_typemap_helper.h"
#include "convert_to_renderpass2.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <unordered_set>

//--------------------------------------------------------------------------------------
// Mesh and VertexFormat Data
//--------------------------------------------------------------------------------------

enum BsoFailSelect {
    BsoFailNone,
    BsoFailLineWidth,
    BsoFailDepthBias,
    BsoFailViewport,
    BsoFailScissor,
    BsoFailBlend,
    BsoFailDepthBounds,
    BsoFailStencilReadMask,
    BsoFailStencilWriteMask,
    BsoFailStencilReference,
    BsoFailCmdClearAttachments,
    BsoFailIndexBuffer,
    BsoFailIndexBufferBadSize,
    BsoFailIndexBufferBadOffset,
    BsoFailIndexBufferBadMapSize,
    BsoFailIndexBufferBadMapOffset
};

// Static arrays helper
template <class ElementT, size_t array_size>
size_t size(ElementT (&)[array_size]) {
    return array_size;
}

// Validation report callback prototype
static VKAPI_ATTR VkBool32 VKAPI_CALL myDbgFunc(VkFlags msgFlags, VkDebugReportObjectTypeEXT objType, uint64_t srcObject,
                                                size_t location, int32_t msgCode, const char *pLayerPrefix, const char *pMsg,
                                                void *pUserData);

// Simple sane SamplerCreateInfo boilerplate
static VkSamplerCreateInfo SafeSaneSamplerCreateInfo() {
    VkSamplerCreateInfo sampler_create_info = {};
    sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_create_info.pNext = nullptr;
    sampler_create_info.magFilter = VK_FILTER_NEAREST;
    sampler_create_info.minFilter = VK_FILTER_NEAREST;
    sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_create_info.mipLodBias = 0.0;
    sampler_create_info.anisotropyEnable = VK_FALSE;
    sampler_create_info.maxAnisotropy = 1.0;
    sampler_create_info.compareEnable = VK_FALSE;
    sampler_create_info.compareOp = VK_COMPARE_OP_NEVER;
    sampler_create_info.minLod = 0.0;
    sampler_create_info.maxLod = 16.0;
    sampler_create_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    sampler_create_info.unnormalizedCoordinates = VK_FALSE;

    return sampler_create_info;
}

static VkImageViewCreateInfo SafeSaneImageViewCreateInfo(VkImage image, VkFormat format, VkImageAspectFlags aspect_mask) {
    VkImageViewCreateInfo image_view_create_info = {};
    image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.image = image;
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = format;
    image_view_create_info.subresourceRange.layerCount = 1;
    image_view_create_info.subresourceRange.baseMipLevel = 0;
    image_view_create_info.subresourceRange.levelCount = 1;
    image_view_create_info.subresourceRange.aspectMask = aspect_mask;

    return image_view_create_info;
}

static VkImageViewCreateInfo SafeSaneImageViewCreateInfo(const VkImageObj &image, VkFormat format, VkImageAspectFlags aspect_mask) {
    return SafeSaneImageViewCreateInfo(image.handle(), format, aspect_mask);
}

// Helper for checking createRenderPass2 support and adding related extensions.
static bool CheckCreateRenderPass2Support(VkRenderFramework *renderFramework, std::vector<const char *> &device_extension_names) {
    if (renderFramework->DeviceExtensionSupported(renderFramework->gpu(), nullptr, VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME)) {
        device_extension_names.push_back(VK_KHR_MULTIVIEW_EXTENSION_NAME);
        device_extension_names.push_back(VK_KHR_MAINTENANCE2_EXTENSION_NAME);
        device_extension_names.push_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
        return true;
    }
    return false;
}
// Helper for checking descriptor_indexing support and adding related extensions.
static bool CheckDescriptorIndexingSupportAndInitFramework(VkRenderFramework *renderFramework,
                                                           std::vector<const char *> &instance_extension_names,
                                                           std::vector<const char *> &device_extension_names,
                                                           VkValidationFeaturesEXT *features, void *userData) {
    bool descriptor_indexing = renderFramework->InstanceExtensionSupported(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    if (descriptor_indexing) {
        instance_extension_names.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }
    renderFramework->InitFramework(myDbgFunc, userData, features);
    descriptor_indexing = descriptor_indexing && renderFramework->DeviceExtensionSupported(renderFramework->gpu(), nullptr,
                                                                                           VK_KHR_MAINTENANCE3_EXTENSION_NAME);
    descriptor_indexing = descriptor_indexing && renderFramework->DeviceExtensionSupported(
                                                     renderFramework->gpu(), nullptr, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
    if (descriptor_indexing) {
        device_extension_names.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
        device_extension_names.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
        return true;
    }
    return false;
}

// Dependent "false" type for the static assert, as GCC will evaluate
// non-dependent static_asserts even for non-instantiated templates
template <typename T>
struct AlwaysFalse : std::false_type {};

// Helpers to get nearest greater or smaller value (of float) -- useful for testing the boundary cases of Vulkan limits
template <typename T>
T NearestGreater(const T from) {
    using Lim = std::numeric_limits<T>;
    const auto positive_direction = Lim::has_infinity ? Lim::infinity() : Lim::max();

    return std::nextafter(from, positive_direction);
}

template <typename T>
T NearestSmaller(const T from) {
    using Lim = std::numeric_limits<T>;
    const auto negative_direction = Lim::has_infinity ? -Lim::infinity() : Lim::lowest();

    return std::nextafter(from, negative_direction);
}

// ErrorMonitor Usage:
//
// Call SetDesiredFailureMsg with a string to be compared against all
// encountered log messages, or a validation error enum identifying
// desired error message. Passing NULL or VALIDATION_ERROR_MAX_ENUM
// will match all log messages. logMsg will return true for skipCall
// only if msg is matched or NULL.
//
// Call VerifyFound to determine if all desired failure messages
// were encountered. Call VerifyNotFound to determine if any unexpected
// failure was encountered.
class ErrorMonitor {
   public:
    ErrorMonitor();
    ~ErrorMonitor();

    // Set monitor to pristine state
    void Reset();

    // ErrorMonitor will look for an error message containing the specified string(s)
    void SetDesiredFailureMsg(const VkFlags msgFlags, const std::string msg);
    void SetDesiredFailureMsg(const VkFlags msgFlags, const char *const msgString);

    // ErrorMonitor will look for an error message containing the specified string(s)
    template <typename Iter>
    void SetDesiredFailureMsg(const VkFlags msgFlags, Iter iter, const Iter end);

    // Set an error that the error monitor will ignore. Do not use this function if you are creating a new test.
    // TODO: This is stopgap to block new unexpected errors from being introduced. The long-term goal is to remove the use of this
    // function and its definition.
    void SetUnexpectedError(const char *const msg);
    VkBool32 CheckForDesiredMsg(const char *const msgString);
    vector<string> GetOtherFailureMsgs() const;
    VkDebugReportFlagsEXT GetMessageFlags() const;
    bool AnyDesiredMsgFound() const;
    bool AllDesiredMsgsFound() const;
    void SetError(const char *const errorString);
    void SetBailout(bool *bailout);

    void DumpFailureMsgs() const;

    // Helpers

    // ExpectSuccess now takes an optional argument allowing a custom combination of debug flags
    void ExpectSuccess(VkDebugReportFlagsEXT const message_flag_mask = VK_DEBUG_REPORT_ERROR_BIT_EXT) {
        // Match ANY message matching specified type
        SetDesiredFailureMsg(message_flag_mask, "");
        message_flags_ = message_flag_mask;  // override mask handling in SetDesired...
    }

    void VerifyFound() {
        // Not receiving expected message(s) is a failure. /Before/ throwing, dump any other messages
        if (!AllDesiredMsgsFound()) {
            DumpFailureMsgs();
            for (const auto desired_msg : desired_message_strings_) {
                ADD_FAILURE() << "Did not receive expected error '" << desired_msg << "'";
            }
        } else if (GetOtherFailureMsgs().size() > 0) {
            // Fail test case for any unexpected errors
#if defined(ANDROID)
            // This will get unexpected errors into the adb log
            for (auto msg : other_messages_) {
                __android_log_print(ANDROID_LOG_INFO, "VulkanLayerValidationTests", "[ UNEXPECTED_ERR ] '%s'", msg.c_str());
            }
#else
            ADD_FAILURE() << "Received unexpected error(s).";
#endif
        }
        Reset();
    }

    void VerifyNotFound() {
        // ExpectSuccess() configured us to match anything. Any error is a failure.
        if (AnyDesiredMsgFound()) {
            DumpFailureMsgs();
            for (const auto msg : failure_message_strings_) {
                ADD_FAILURE() << "Expected to succeed but got error: " << msg;
            }
        } else if (GetOtherFailureMsgs().size() > 0) {
            // Fail test case for any unexpected errors
#if defined(ANDROID)
            // This will get unexpected errors into the adb log
            for (auto msg : other_messages_) {
                __android_log_print(ANDROID_LOG_INFO, "VulkanLayerValidationTests", "[ UNEXPECTED_ERR ] '%s'", msg.c_str());
            }
#else
            ADD_FAILURE() << "Received unexpected error(s).";
#endif
        }
        Reset();
    }

   private:
    // TODO: This is stopgap to block new unexpected errors from being introduced. The long-term goal is to remove the use of this
    // function and its definition.
    bool IgnoreMessage(std::string const &msg) const {
        if (ignore_message_strings_.empty()) {
            return false;
        }

        return std::find_if(ignore_message_strings_.begin(), ignore_message_strings_.end(), [&msg](std::string const &str) {
                   return msg.find(str) != std::string::npos;
               }) != ignore_message_strings_.end();
    }

    VkFlags message_flags_;
    std::unordered_multiset<std::string> desired_message_strings_;
    std::unordered_multiset<std::string> failure_message_strings_;
    std::vector<std::string> ignore_message_strings_;
    vector<string> other_messages_;
    test_platform_thread_mutex mutex_;
    bool *bailout_;
    bool message_found_;
};

static VKAPI_ATTR VkBool32 VKAPI_CALL myDbgFunc(VkFlags msgFlags, VkDebugReportObjectTypeEXT objType, uint64_t srcObject,
                                                size_t location, int32_t msgCode, const char *pLayerPrefix, const char *pMsg,
                                                void *pUserData) {
    ErrorMonitor *errMonitor = (ErrorMonitor *)pUserData;
    if (msgFlags & errMonitor->GetMessageFlags()) {
        return errMonitor->CheckForDesiredMsg(pMsg);
    }
    return VK_FALSE;
}

class VkLayerTest : public VkRenderFramework {
   public:
    void VKTriangleTest(BsoFailSelect failCase);
    void GenericDrawPreparation(VkCommandBufferObj *commandBuffer, VkPipelineObj &pipelineobj, VkDescriptorSetObj &descriptorSet,
                                BsoFailSelect failCase);

    void Init(VkPhysicalDeviceFeatures *features = nullptr, VkPhysicalDeviceFeatures2 *features2 = nullptr,
              const VkCommandPoolCreateFlags flags = 0, void *instance_pnext = nullptr) {
        InitFramework(myDbgFunc, m_errorMonitor, instance_pnext);
        InitState(features, features2, flags);
    }

    ErrorMonitor *Monitor() { return m_errorMonitor; }
    VkCommandBufferObj *CommandBuffer() { return m_commandBuffer; }

    // Format search helper
    VkFormat FindSupportedDepthStencilFormat(VkPhysicalDevice phy) {
        VkFormat ds_formats[] = {VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT};
        for (uint32_t i = 0; i < sizeof(ds_formats); i++) {
            VkFormatProperties format_props;
            vkGetPhysicalDeviceFormatProperties(phy, ds_formats[i], &format_props);

            if (format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                return ds_formats[i];
            }
        }
        return VK_FORMAT_UNDEFINED;
    }

    // Returns true if *any* requested features are available.
    // Assumption is that the framework can successfully create an image as
    // long as at least one of the feature bits is present (excepting VTX_BUF).
    bool ImageFormatIsSupported(VkPhysicalDevice phy, VkFormat format, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
                                VkFormatFeatureFlags features = ~VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT) {
        VkFormatProperties format_props;
        vkGetPhysicalDeviceFormatProperties(phy, format, &format_props);
        VkFormatFeatureFlags phy_features =
            (VK_IMAGE_TILING_OPTIMAL == tiling ? format_props.optimalTilingFeatures : format_props.linearTilingFeatures);
        return (0 != (phy_features & features));
    }

    // Returns true if format and *all* requested features are available.
    bool ImageFormatAndFeaturesSupported(VkPhysicalDevice phy, VkFormat format, VkImageTiling tiling,
                                         VkFormatFeatureFlags features) {
        VkFormatProperties format_props;
        vkGetPhysicalDeviceFormatProperties(phy, format, &format_props);
        VkFormatFeatureFlags phy_features =
            (VK_IMAGE_TILING_OPTIMAL == tiling ? format_props.optimalTilingFeatures : format_props.linearTilingFeatures);
        return (features == (phy_features & features));
    }

    // Returns true if format and *all* requested features are available.
    bool ImageFormatAndFeaturesSupported(const VkInstance inst, const VkPhysicalDevice phy, const VkImageCreateInfo info,
                                         const VkFormatFeatureFlags features) {
        // Verify physical device support of format features
        if (!ImageFormatAndFeaturesSupported(phy, info.format, info.tiling, features)) {
            return false;
        }

        // Verify that PhysDevImageFormatProp() also claims support for the specific usage
        VkImageFormatProperties props;
        VkResult err =
            vkGetPhysicalDeviceImageFormatProperties(phy, info.format, info.imageType, info.tiling, info.usage, info.flags, &props);
        if (VK_SUCCESS != err) {
            return false;
        }

#if 0  // Convinced this chunk doesn't currently add any additional info, but leaving in place because it may be
       // necessary with future extensions

    // Verify again using version 2, if supported, which *can* return more property data than the original...
    // (It's not clear that this is any more definitive than using the original version - but no harm)
    PFN_vkGetPhysicalDeviceImageFormatProperties2KHR p_GetPDIFP2KHR =
        (PFN_vkGetPhysicalDeviceImageFormatProperties2KHR)vkGetInstanceProcAddr(inst,
                                                                                "vkGetPhysicalDeviceImageFormatProperties2KHR");
    if (NULL != p_GetPDIFP2KHR) {
        VkPhysicalDeviceImageFormatInfo2KHR fmt_info{};
        fmt_info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2_KHR;
        fmt_info.pNext = nullptr;
        fmt_info.format = info.format;
        fmt_info.type = info.imageType;
        fmt_info.tiling = info.tiling;
        fmt_info.usage = info.usage;
        fmt_info.flags = info.flags;

        VkImageFormatProperties2KHR fmt_props = {};
        fmt_props.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2_KHR;
        err = p_GetPDIFP2KHR(phy, &fmt_info, &fmt_props);
        if (VK_SUCCESS != err) {
            return false;
        }
    }
#endif

        return true;
    }

   protected:
    ErrorMonitor *m_errorMonitor;
    uint32_t m_instance_api_version = 0;
    uint32_t m_target_api_version = 0;
    bool m_enableWSI;

    virtual void SetUp() {
        m_instance_layer_names.clear();
        m_instance_extension_names.clear();
        m_device_extension_names.clear();

        // Add default instance extensions to the list
        m_instance_extension_names.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

        if (VkTestFramework::m_khronos_layer_disable) {
            m_instance_layer_names.push_back("VK_LAYER_GOOGLE_threading");
            m_instance_layer_names.push_back("VK_LAYER_LUNARG_parameter_validation");
            m_instance_layer_names.push_back("VK_LAYER_LUNARG_object_tracker");
            m_instance_layer_names.push_back("VK_LAYER_LUNARG_core_validation");
            m_instance_layer_names.push_back("VK_LAYER_GOOGLE_unique_objects");
        } else {
            m_instance_layer_names.push_back("VK_LAYER_KHRONOS_validation");
        }
        if (VkTestFramework::m_devsim_layer) {
            if (InstanceLayerSupported("VK_LAYER_LUNARG_device_simulation")) {
                m_instance_layer_names.push_back("VK_LAYER_LUNARG_device_simulation");
            } else {
                VkTestFramework::m_devsim_layer = false;
                printf("             Did not find VK_LAYER_LUNARG_device_simulation layer so it will not be enabled.\n");
            }
        }
        if (m_enableWSI) {
            m_instance_extension_names.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
            m_device_extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#ifdef NEED_TO_TEST_THIS_ON_PLATFORM
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
            m_instance_extension_names.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#endif  // VK_USE_PLATFORM_ANDROID_KHR
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
            m_instance_extension_names.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#endif  // VK_USE_PLATFORM_WAYLAND_KHR
#if defined(VK_USE_PLATFORM_WIN32_KHR)
            m_instance_extension_names.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif  // VK_USE_PLATFORM_WIN32_KHR
#endif  // NEED_TO_TEST_THIS_ON_PLATFORM
#if defined(VK_USE_PLATFORM_XCB_KHR)
            m_instance_extension_names.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
            m_instance_extension_names.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif  // VK_USE_PLATFORM_XLIB_KHR
        }

        this->app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        this->app_info.pNext = NULL;
        this->app_info.pApplicationName = "layer_tests";
        this->app_info.applicationVersion = 1;
        this->app_info.pEngineName = "unittest";
        this->app_info.engineVersion = 1;
        this->app_info.apiVersion = VK_API_VERSION_1_0;

        m_errorMonitor = new ErrorMonitor;

        // Find out what version the instance supports and record the default target instance
        auto enumerateInstanceVersion =
            (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion");
        if (enumerateInstanceVersion) {
            enumerateInstanceVersion(&m_instance_api_version);
        } else {
            m_instance_api_version = VK_API_VERSION_1_0;
        }
        m_target_api_version = app_info.apiVersion;
    }

    uint32_t SetTargetApiVersion(uint32_t target_api_version) {
        if (target_api_version == 0) target_api_version = VK_API_VERSION_1_0;
        if (target_api_version <= m_instance_api_version) {
            m_target_api_version = target_api_version;
            app_info.apiVersion = m_target_api_version;
        }
        return m_target_api_version;
    }
    uint32_t DeviceValidationVersion() {
        // The validation layers, assume the version we are validating to is the apiVersion unless the device apiVersion is lower
        VkPhysicalDeviceProperties props;
        GetPhysicalDeviceProperties(&props);
        return std::min(m_target_api_version, props.apiVersion);
    }

    bool LoadDeviceProfileLayer(
        PFN_vkSetPhysicalDeviceFormatPropertiesEXT &fpvkSetPhysicalDeviceFormatPropertiesEXT,
        PFN_vkGetOriginalPhysicalDeviceFormatPropertiesEXT &fpvkGetOriginalPhysicalDeviceFormatPropertiesEXT) {
        // Load required functions
        fpvkSetPhysicalDeviceFormatPropertiesEXT =
            (PFN_vkSetPhysicalDeviceFormatPropertiesEXT)vkGetInstanceProcAddr(instance(), "vkSetPhysicalDeviceFormatPropertiesEXT");
        fpvkGetOriginalPhysicalDeviceFormatPropertiesEXT =
            (PFN_vkGetOriginalPhysicalDeviceFormatPropertiesEXT)vkGetInstanceProcAddr(
                instance(), "vkGetOriginalPhysicalDeviceFormatPropertiesEXT");

        if (!(fpvkSetPhysicalDeviceFormatPropertiesEXT) || !(fpvkGetOriginalPhysicalDeviceFormatPropertiesEXT)) {
            printf("%s Can't find device_profile_api functions; skipped.\n", kSkipPrefix);
            return 0;
        }

        return 1;
    }

    virtual void TearDown() {
        // Clean up resources before we reset
        ShutdownFramework();
        delete m_errorMonitor;
    }

    VkLayerTest() { m_enableWSI = false; }
};

void VkLayerTest::VKTriangleTest(BsoFailSelect failCase) {
    ASSERT_TRUE(m_device && m_device->initialized());  // VKTriangleTest assumes Init() has finished

    ASSERT_NO_FATAL_FAILURE(InitViewport());

    CreatePipelineHelper helper(*this);
    helper.InitInfo();

    bool failcase_needs_depth = false;  // to mark cases that need depth attachment

    VkBufferObj index_buffer;

    switch (failCase) {
        case BsoFailLineWidth: {
            pipelineobj.MakeDynamic(VK_DYNAMIC_STATE_LINE_WIDTH);
            VkPipelineInputAssemblyStateCreateInfo ia_state = {};
            ia_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            ia_state.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            pipelineobj.SetInputAssembly(&ia_state);
            break;
        }
        case BsoFailDepthBias: {
            pipelineobj.MakeDynamic(VK_DYNAMIC_STATE_DEPTH_BIAS);
            VkPipelineRasterizationStateCreateInfo rs_state = {};
            rs_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rs_state.depthBiasEnable = VK_TRUE;
            rs_state.lineWidth = 1.0f;
            pipelineobj.SetRasterization(&rs_state);
            break;
        }
        case BsoFailViewport: {
            pipelineobj.MakeDynamic(VK_DYNAMIC_STATE_VIEWPORT);
            break;
        }
        case BsoFailScissor: {
            pipelineobj.MakeDynamic(VK_DYNAMIC_STATE_SCISSOR);
            break;
        }
        case BsoFailBlend: {
            pipelineobj.MakeDynamic(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
            VkPipelineColorBlendAttachmentState att_state = {};
            att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_CONSTANT_COLOR;
            att_state.blendEnable = VK_TRUE;
            pipelineobj.AddColorAttachment(0, att_state);
            break;
        }
        case BsoFailDepthBounds: {
            failcase_needs_depth = true;
            pipelineobj.MakeDynamic(VK_DYNAMIC_STATE_DEPTH_BOUNDS);
            break;
        }
        case BsoFailStencilReadMask: {
            failcase_needs_depth = true;
            pipelineobj.MakeDynamic(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
            break;
        }
        case BsoFailStencilWriteMask: {
            failcase_needs_depth = true;
            pipelineobj.MakeDynamic(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
            break;
        }
        case BsoFailStencilReference: {
            failcase_needs_depth = true;
            pipelineobj.MakeDynamic(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
            break;
        }

        case BsoFailIndexBuffer:
            break;
        case BsoFailIndexBufferBadSize:
        case BsoFailIndexBufferBadOffset:
        case BsoFailIndexBufferBadMapSize:
        case BsoFailIndexBufferBadMapOffset: {
            // Create an index buffer for these tests.
            // There is no need to populate it because we should bail before trying to draw.
            uint32_t const indices[] = {0};
            VkBufferCreateInfo buffer_info = {};
            buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_info.size = 1024;
            buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            buffer_info.queueFamilyIndexCount = 1;
            buffer_info.pQueueFamilyIndices = indices;
            index_buffer.init(*m_device, buffer_info, (VkMemoryPropertyFlags)VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        } break;
        case BsoFailCmdClearAttachments:
            break;
        case BsoFailNone:
            break;
        default:
            break;
    }

    VkDescriptorSetObj descriptorSet(m_device);

    VkImageView *depth_attachment = nullptr;
    if (failcase_needs_depth) {
        m_depth_stencil_fmt = FindSupportedDepthStencilFormat(gpu());
        ASSERT_TRUE(m_depth_stencil_fmt != VK_FORMAT_UNDEFINED);

        m_depthStencil->Init(m_device, static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), m_depth_stencil_fmt,
                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        depth_attachment = m_depthStencil->BindInfo();
    }

    ASSERT_NO_FATAL_FAILURE(InitRenderTarget(1, depth_attachment));
    m_commandBuffer->begin();

    GenericDrawPreparation(m_commandBuffer, pipelineobj, descriptorSet, failCase);

    m_commandBuffer->BeginRenderPass(m_renderPassBeginInfo);

    // render triangle
    if (failCase == BsoFailIndexBuffer) {
        // Use DrawIndexed w/o an index buffer bound
        m_commandBuffer->DrawIndexed(3, 1, 0, 0, 0);
    } else if (failCase == BsoFailIndexBufferBadSize) {
        // Bind the index buffer and draw one too many indices
        m_commandBuffer->BindIndexBuffer(&index_buffer, 0, VK_INDEX_TYPE_UINT16);
        m_commandBuffer->DrawIndexed(513, 1, 0, 0, 0);
    } else if (failCase == BsoFailIndexBufferBadOffset) {
        // Bind the index buffer and draw one past the end of the buffer using the offset
        m_commandBuffer->BindIndexBuffer(&index_buffer, 0, VK_INDEX_TYPE_UINT16);
        m_commandBuffer->DrawIndexed(512, 1, 1, 0, 0);
    } else if (failCase == BsoFailIndexBufferBadMapSize) {
        // Bind the index buffer at the middle point and draw one too many indices
        m_commandBuffer->BindIndexBuffer(&index_buffer, 512, VK_INDEX_TYPE_UINT16);
        m_commandBuffer->DrawIndexed(257, 1, 0, 0, 0);
    } else if (failCase == BsoFailIndexBufferBadMapOffset) {
        // Bind the index buffer at the middle point and draw one past the end of the buffer
        m_commandBuffer->BindIndexBuffer(&index_buffer, 512, VK_INDEX_TYPE_UINT16);
        m_commandBuffer->DrawIndexed(256, 1, 1, 0, 0);
    } else {
        m_commandBuffer->Draw(3, 1, 0, 0);
    }

    if (failCase == BsoFailCmdClearAttachments) {
        VkClearAttachment color_attachment = {};
        color_attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        color_attachment.colorAttachment = 2000000000;  // Someone who knew what they were doing would use 0 for the index;
        VkClearRect clear_rect = {{{0, 0}, {static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height)}}, 0, 0};

        vkCmdClearAttachments(m_commandBuffer->handle(), 1, &color_attachment, 1, &clear_rect);
    }

    // finalize recording of the command buffer
    m_commandBuffer->EndRenderPass();
    m_commandBuffer->end();
    m_commandBuffer->QueueCommandBuffer(true);
    DestroyRenderTarget();
}

void VkLayerTest::GenericDrawPreparation(VkCommandBufferObj *commandBuffer, VkPipelineObj &pipelineobj,
                                         VkDescriptorSetObj &descriptorSet, BsoFailSelect failCase) {
    commandBuffer->ClearAllBuffers(m_renderTargets, m_clear_color, m_depthStencil, m_depth_clear_color, m_stencil_clear_color);

    commandBuffer->PrepareAttachments(m_renderTargets, m_depthStencil);
    // Make sure depthWriteEnable is set so that Depth fail test will work
    // correctly
    // Make sure stencilTestEnable is set so that Stencil fail test will work
    // correctly
    VkStencilOpState stencil = {};
    stencil.failOp = VK_STENCIL_OP_KEEP;
    stencil.passOp = VK_STENCIL_OP_KEEP;
    stencil.depthFailOp = VK_STENCIL_OP_KEEP;
    stencil.compareOp = VK_COMPARE_OP_NEVER;

    VkPipelineDepthStencilStateCreateInfo ds_ci = {};
    ds_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds_ci.pNext = NULL;
    ds_ci.depthTestEnable = VK_FALSE;
    ds_ci.depthWriteEnable = VK_TRUE;
    ds_ci.depthCompareOp = VK_COMPARE_OP_NEVER;
    ds_ci.depthBoundsTestEnable = VK_FALSE;
    if (failCase == BsoFailDepthBounds) {
        ds_ci.depthBoundsTestEnable = VK_TRUE;
        ds_ci.maxDepthBounds = 0.0f;
        ds_ci.minDepthBounds = 0.0f;
    }
    ds_ci.stencilTestEnable = VK_TRUE;
    ds_ci.front = stencil;
    ds_ci.back = stencil;

    pipelineobj.SetDepthStencil(&ds_ci);
    pipelineobj.SetViewport(m_viewports);
    pipelineobj.SetScissor(m_scissors);
    descriptorSet.CreateVKDescriptorSet(commandBuffer);
    VkResult err = pipelineobj.CreateVKPipeline(descriptorSet.GetPipelineLayout(), renderPass());
    ASSERT_VK_SUCCESS(err);
    vkCmdBindPipeline(commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineobj.handle());
    commandBuffer->BindDescriptorSet(descriptorSet);
}

class VkPositiveLayerTest : public VkLayerTest {
   public:
   protected:
};

class VkWsiEnabledLayerTest : public VkLayerTest {
   public:
   protected:
    VkWsiEnabledLayerTest() { m_enableWSI = true; }
};

class VkBufferTest {
   public:
    enum eTestEnFlags {
        eDoubleDelete,
        eInvalidDeviceOffset,
        eInvalidMemoryOffset,
        eBindNullBuffer,
        eBindFakeBuffer,
        eFreeInvalidHandle,
        eNone,
    };

    enum eTestConditions { eOffsetAlignment = 1 };

    static bool GetTestConditionValid(VkDeviceObj *aVulkanDevice, eTestEnFlags aTestFlag, VkBufferUsageFlags aBufferUsage = 0) {
        if (eInvalidDeviceOffset != aTestFlag && eInvalidMemoryOffset != aTestFlag) {
            return true;
        }
        VkDeviceSize offset_limit = 0;
        if (eInvalidMemoryOffset == aTestFlag) {
            VkBuffer vulkanBuffer;
            VkBufferCreateInfo buffer_create_info = {};
            buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_create_info.size = 32;
            buffer_create_info.usage = aBufferUsage;

            vkCreateBuffer(aVulkanDevice->device(), &buffer_create_info, nullptr, &vulkanBuffer);
            VkMemoryRequirements memory_reqs = {};

            vkGetBufferMemoryRequirements(aVulkanDevice->device(), vulkanBuffer, &memory_reqs);
            vkDestroyBuffer(aVulkanDevice->device(), vulkanBuffer, nullptr);
            offset_limit = memory_reqs.alignment;
        } else if ((VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT) & aBufferUsage) {
            offset_limit = aVulkanDevice->props.limits.minTexelBufferOffsetAlignment;
        } else if (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT & aBufferUsage) {
            offset_limit = aVulkanDevice->props.limits.minUniformBufferOffsetAlignment;
        } else if (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT & aBufferUsage) {
            offset_limit = aVulkanDevice->props.limits.minStorageBufferOffsetAlignment;
        }
        return eOffsetAlignment < offset_limit;
    }

    // A constructor which performs validation tests within construction.
    VkBufferTest(VkDeviceObj *aVulkanDevice, VkBufferUsageFlags aBufferUsage, eTestEnFlags aTestFlag = eNone)
        : AllocateCurrent(true),
          BoundCurrent(false),
          CreateCurrent(false),
          InvalidDeleteEn(false),
          VulkanDevice(aVulkanDevice->device()) {
        if (eBindNullBuffer == aTestFlag || eBindFakeBuffer == aTestFlag) {
            VkMemoryAllocateInfo memory_allocate_info = {};
            memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            memory_allocate_info.allocationSize = 1;   // fake size -- shouldn't matter for the test
            memory_allocate_info.memoryTypeIndex = 0;  // fake type -- shouldn't matter for the test
            vkAllocateMemory(VulkanDevice, &memory_allocate_info, nullptr, &VulkanMemory);

            VulkanBuffer = (aTestFlag == eBindNullBuffer) ? VK_NULL_HANDLE : (VkBuffer)0xCDCDCDCDCDCDCDCD;

            vkBindBufferMemory(VulkanDevice, VulkanBuffer, VulkanMemory, 0);
        } else {
            VkBufferCreateInfo buffer_create_info = {};
            buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_create_info.size = 32;
            buffer_create_info.usage = aBufferUsage;

            vkCreateBuffer(VulkanDevice, &buffer_create_info, nullptr, &VulkanBuffer);

            CreateCurrent = true;

            VkMemoryRequirements memory_requirements;
            vkGetBufferMemoryRequirements(VulkanDevice, VulkanBuffer, &memory_requirements);

            VkMemoryAllocateInfo memory_allocate_info = {};
            memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            memory_allocate_info.allocationSize = memory_requirements.size + eOffsetAlignment;
            bool pass = aVulkanDevice->phy().set_memory_type(memory_requirements.memoryTypeBits, &memory_allocate_info,
                                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            if (!pass) {
                CreateCurrent = false;
                vkDestroyBuffer(VulkanDevice, VulkanBuffer, nullptr);
                return;
            }

            vkAllocateMemory(VulkanDevice, &memory_allocate_info, NULL, &VulkanMemory);
            // NB: 1 is intentionally an invalid offset value
            const bool offset_en = eInvalidDeviceOffset == aTestFlag || eInvalidMemoryOffset == aTestFlag;
            vkBindBufferMemory(VulkanDevice, VulkanBuffer, VulkanMemory, offset_en ? eOffsetAlignment : 0);
            BoundCurrent = true;

            InvalidDeleteEn = (eFreeInvalidHandle == aTestFlag);
        }
    }

    ~VkBufferTest() {
        if (CreateCurrent) {
            vkDestroyBuffer(VulkanDevice, VulkanBuffer, nullptr);
        }
        if (AllocateCurrent) {
            if (InvalidDeleteEn) {
                union {
                    VkDeviceMemory device_memory;
                    unsigned long long index_access;
                } bad_index;

                bad_index.device_memory = VulkanMemory;
                bad_index.index_access++;

                vkFreeMemory(VulkanDevice, bad_index.device_memory, nullptr);
            }
            vkFreeMemory(VulkanDevice, VulkanMemory, nullptr);
        }
    }

    bool GetBufferCurrent() { return AllocateCurrent && BoundCurrent && CreateCurrent; }

    const VkBuffer &GetBuffer() { return VulkanBuffer; }

    void TestDoubleDestroy() {
        // Destroy the buffer but leave the flag set, which will cause
        // the buffer to be destroyed again in the destructor.
        vkDestroyBuffer(VulkanDevice, VulkanBuffer, nullptr);
    }

   protected:
    bool AllocateCurrent;
    bool BoundCurrent;
    bool CreateCurrent;
    bool InvalidDeleteEn;

    VkBuffer VulkanBuffer;
    VkDevice VulkanDevice;
    VkDeviceMemory VulkanMemory;
};

class VkVerticesObj {
   public:
    VkVerticesObj(VkDeviceObj *aVulkanDevice, unsigned aAttributeCount, unsigned aBindingCount, unsigned aByteStride,
                  VkDeviceSize aVertexCount, const float *aVerticies)
        : BoundCurrent(false),
          AttributeCount(aAttributeCount),
          BindingCount(aBindingCount),
          BindId(BindIdGenerator),
          PipelineVertexInputStateCreateInfo(),
          VulkanMemoryBuffer(aVulkanDevice, static_cast<int>(aByteStride * aVertexCount),
                             reinterpret_cast<const void *>(aVerticies), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) {
        BindIdGenerator++;  // NB: This can wrap w/misuse

        VertexInputAttributeDescription = new VkVertexInputAttributeDescription[AttributeCount];
        VertexInputBindingDescription = new VkVertexInputBindingDescription[BindingCount];

        PipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = VertexInputAttributeDescription;
        PipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = AttributeCount;
        PipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = VertexInputBindingDescription;
        PipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = BindingCount;
        PipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        unsigned i = 0;
        do {
            VertexInputAttributeDescription[i].binding = BindId;
            VertexInputAttributeDescription[i].location = i;
            VertexInputAttributeDescription[i].format = VK_FORMAT_R32G32B32_SFLOAT;
            VertexInputAttributeDescription[i].offset = sizeof(float) * aByteStride;
            i++;
        } while (AttributeCount < i);

        i = 0;
        do {
            VertexInputBindingDescription[i].binding = BindId;
            VertexInputBindingDescription[i].stride = aByteStride;
            VertexInputBindingDescription[i].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            i++;
        } while (BindingCount < i);
    }

    ~VkVerticesObj() {
        if (VertexInputAttributeDescription) {
            delete[] VertexInputAttributeDescription;
        }
        if (VertexInputBindingDescription) {
            delete[] VertexInputBindingDescription;
        }
    }

    bool AddVertexInputToPipe(VkPipelineObj &aPipelineObj) {
        aPipelineObj.AddVertexInputAttribs(VertexInputAttributeDescription, AttributeCount);
        aPipelineObj.AddVertexInputBindings(VertexInputBindingDescription, BindingCount);
        return true;
    }

    void BindVertexBuffers(VkCommandBuffer aCommandBuffer, unsigned aOffsetCount = 0, VkDeviceSize *aOffsetList = nullptr) {
        VkDeviceSize *offsetList;
        unsigned offsetCount;

        if (aOffsetCount) {
            offsetList = aOffsetList;
            offsetCount = aOffsetCount;
        } else {
            offsetList = new VkDeviceSize[1]();
            offsetCount = 1;
        }

        vkCmdBindVertexBuffers(aCommandBuffer, BindId, offsetCount, &VulkanMemoryBuffer.handle(), offsetList);
        BoundCurrent = true;

        if (!aOffsetCount) {
            delete[] offsetList;
        }
    }

   protected:
    static uint32_t BindIdGenerator;

    bool BoundCurrent;
    unsigned AttributeCount;
    unsigned BindingCount;
    uint32_t BindId;

    VkPipelineVertexInputStateCreateInfo PipelineVertexInputStateCreateInfo;
    VkVertexInputAttributeDescription *VertexInputAttributeDescription;
    VkVertexInputBindingDescription *VertexInputBindingDescription;
    VkConstantBufferObj VulkanMemoryBuffer;
};

uint32_t VkVerticesObj::BindIdGenerator;

struct OneOffDescriptorSet {
    VkDeviceObj *device_;
    VkDescriptorPool pool_;
    VkDescriptorSetLayoutObj layout_;
    VkDescriptorSet set_;
    typedef std::vector<VkDescriptorSetLayoutBinding> Bindings;

    OneOffDescriptorSet(VkDeviceObj *device, const Bindings &bindings, VkDescriptorSetLayoutCreateFlags layout_flags = 0,
                        void *layout_pnext = NULL, VkDescriptorPoolCreateFlags poolFlags = 0, void *allocate_pnext = NULL)
        : device_{device}, pool_{}, layout_(device, bindings, layout_flags, layout_pnext), set_{} {
        VkResult err;

        std::vector<VkDescriptorPoolSize> sizes;
        for (const auto &b : bindings) sizes.push_back({b.descriptorType, std::max(1u, b.descriptorCount)});

        VkDescriptorPoolCreateInfo dspci = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, poolFlags, 1, uint32_t(sizes.size()), sizes.data()};
        err = vkCreateDescriptorPool(device_->handle(), &dspci, nullptr, &pool_);
        if (err != VK_SUCCESS) return;

        VkDescriptorSetAllocateInfo alloc_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, allocate_pnext, pool_, 1,
                                                  &layout_.handle()};
        err = vkAllocateDescriptorSets(device_->handle(), &alloc_info, &set_);
    }

    ~OneOffDescriptorSet() {
        // No need to destroy set-- it's going away with the pool.
        vkDestroyDescriptorPool(device_->handle(), pool_, nullptr);
    }

    bool Initialized() { return pool_ != VK_NULL_HANDLE && layout_.initialized() && set_ != VK_NULL_HANDLE; }
};

template <typename T>
bool IsValidVkStruct(const T &s) {
    return LvlTypeMap<T>::kSType == s.sType;
}

// Helper class for tersely creating create pipeline tests
//
// Designed with minimal error checking to ensure easy error state creation
// See OneshotTest for typical usage
struct CreatePipelineHelper {
   public:
    std::vector<VkDescriptorSetLayoutBinding> dsl_bindings_;
    std::unique_ptr<OneOffDescriptorSet> descriptor_set_;
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages_;
    VkPipelineVertexInputStateCreateInfo vi_ci_ = {};
    VkPipelineInputAssemblyStateCreateInfo ia_ci_ = {};
    VkPipelineTessellationStateCreateInfo tess_ci_ = {};
    VkViewport viewport_ = {};
    VkRect2D scissor_ = {};
    VkPipelineViewportStateCreateInfo vp_state_ci_ = {};
    VkPipelineMultisampleStateCreateInfo pipe_ms_state_ci_ = {};
    VkPipelineLayoutCreateInfo pipeline_layout_ci_ = {};
    VkPipelineLayoutObj pipeline_layout_;
    VkPipelineDynamicStateCreateInfo dyn_state_ci_ = {};
    VkPipelineRasterizationStateCreateInfo rs_state_ci_ = {};
    VkPipelineColorBlendAttachmentState cb_attachments_ = {};
    VkPipelineColorBlendStateCreateInfo cb_ci_ = {};
    VkGraphicsPipelineCreateInfo gp_ci_ = {};
    VkPipelineCacheCreateInfo pc_ci_ = {};
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipelineCache pipeline_cache_ = VK_NULL_HANDLE;
    std::unique_ptr<VkShaderObj> vs_;
    std::unique_ptr<VkShaderObj> fs_;
    VkLayerTest &layer_test_;
    CreatePipelineHelper(VkLayerTest &test) : layer_test_(test) {}
    ~CreatePipelineHelper() {
        VkDevice device = layer_test_.device();
        vkDestroyPipelineCache(device, pipeline_cache_, nullptr);
        vkDestroyPipeline(device, pipeline_, nullptr);
    }

    void InitDescriptorSetInfo() { dsl_bindings_ = {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr}}; }

    void InitInputAndVertexInfo() {
        vi_ci_.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        ia_ci_.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia_ci_.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    }

    void InitMultisampleInfo() {
        pipe_ms_state_ci_.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        pipe_ms_state_ci_.pNext = nullptr;
        pipe_ms_state_ci_.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        pipe_ms_state_ci_.sampleShadingEnable = VK_FALSE;
        pipe_ms_state_ci_.minSampleShading = 1.0;
        pipe_ms_state_ci_.pSampleMask = NULL;
    }

    void InitPipelineLayoutInfo() {
        pipeline_layout_ci_.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_ci_.setLayoutCount = 1;     // Not really changeable because InitState() sets exactly one pSetLayout
        pipeline_layout_ci_.pSetLayouts = nullptr;  // must bound after it is created
    }

    void InitViewportInfo() {
        viewport_ = {0.0f, 0.0f, 64.0f, 64.0f, 0.0f, 1.0f};
        scissor_ = {{0, 0}, {64, 64}};

        vp_state_ci_.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vp_state_ci_.pNext = nullptr;
        vp_state_ci_.viewportCount = 1;
        vp_state_ci_.pViewports = &viewport_;  // ignored if dynamic
        vp_state_ci_.scissorCount = 1;
        vp_state_ci_.pScissors = &scissor_;  // ignored if dynamic
    }

    void InitDynamicStateInfo() {
        // Use a "validity" check on the {} initialized structure to detect initialization
        // during late bind
    }

    void InitShaderInfo() {
        vs_.reset(new VkShaderObj(layer_test_.DeviceObj(), bindStateVertShaderText, VK_SHADER_STAGE_VERTEX_BIT, &layer_test_));
        fs_.reset(new VkShaderObj(layer_test_.DeviceObj(), bindStateFragShaderText, VK_SHADER_STAGE_FRAGMENT_BIT, &layer_test_));
        // We shouldn't need a fragment shader but add it to be able to run on more devices
        shader_stages_ = {vs_->GetStageCreateInfo(), fs_->GetStageCreateInfo()};
    }

    void InitRasterizationInfo() {
        rs_state_ci_.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs_state_ci_.pNext = nullptr;
        rs_state_ci_.flags = 0;
        rs_state_ci_.depthClampEnable = VK_FALSE;
        rs_state_ci_.rasterizerDiscardEnable = VK_FALSE;
        rs_state_ci_.polygonMode = VK_POLYGON_MODE_FILL;
        rs_state_ci_.cullMode = VK_CULL_MODE_BACK_BIT;
        rs_state_ci_.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rs_state_ci_.depthBiasEnable = VK_FALSE;
        rs_state_ci_.lineWidth = 1.0F;
    }

    void InitBlendStateInfo() {
        cb_ci_.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb_ci_.logicOpEnable = VK_FALSE;
        cb_ci_.logicOp = VK_LOGIC_OP_COPY;  // ignored if enable is VK_FALSE above
        cb_ci_.attachmentCount = layer_test_.RenderPassInfo().subpassCount;
        ASSERT_TRUE(IsValidVkStruct(layer_test_.RenderPassInfo()));
        cb_ci_.pAttachments = &cb_attachments_;
        for (int i = 0; i < 4; i++) {
            cb_ci_.blendConstants[0] = 1.0F;
        }
    }

    void InitGraphicsPipelineInfo() {
        // Color-only rendering in a subpass with no depth/stencil attachment
        // Active Pipeline Shader Stages
        //    Vertex Shader
        //    Fragment Shader
        // Required: Fixed-Function Pipeline Stages
        //    VkPipelineVertexInputStateCreateInfo
        //    VkPipelineInputAssemblyStateCreateInfo
        //    VkPipelineViewportStateCreateInfo
        //    VkPipelineRasterizationStateCreateInfo
        //    VkPipelineMultisampleStateCreateInfo
        //    VkPipelineColorBlendStateCreateInfo
        gp_ci_.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gp_ci_.pNext = nullptr;
        gp_ci_.flags = VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;
        gp_ci_.pVertexInputState = &vi_ci_;
        gp_ci_.pInputAssemblyState = &ia_ci_;
        gp_ci_.pTessellationState = nullptr;
        gp_ci_.pViewportState = &vp_state_ci_;
        gp_ci_.pRasterizationState = &rs_state_ci_;
        gp_ci_.pMultisampleState = &pipe_ms_state_ci_;
        gp_ci_.pDepthStencilState = nullptr;
        gp_ci_.pColorBlendState = &cb_ci_;
        gp_ci_.pDynamicState = nullptr;
        gp_ci_.renderPass = layer_test_.renderPass();
    }

    void InitPipelineCacheInfo() {
        pc_ci_.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        pc_ci_.pNext = nullptr;
        pc_ci_.flags = 0;
        pc_ci_.initialDataSize = 0;
        pc_ci_.pInitialData = nullptr;
    }

    // Not called by default during init_info
    void InitTesselationState() {
        // TBD -- add shaders and create_info
    }

    // TDB -- add control for optional and/or additional initialization
    void InitInfo() {
        InitDescriptorSetInfo();
        InitInputAndVertexInfo();
        InitMultisampleInfo();
        InitPipelineLayoutInfo();
        InitViewportInfo();
        InitDynamicStateInfo();
        InitShaderInfo();
        InitRasterizationInfo();
        InitBlendStateInfo();
        InitGraphicsPipelineInfo();
        InitPipelineCacheInfo();
    }

    void InitState() {
        VkResult err;
        descriptor_set_.reset(new OneOffDescriptorSet(layer_test_.DeviceObj(), dsl_bindings_));
        ASSERT_TRUE(descriptor_set_->Initialized());

        const std::vector<VkPushConstantRange> push_ranges(
            pipeline_layout_ci_.pPushConstantRanges,
            pipeline_layout_ci_.pPushConstantRanges + pipeline_layout_ci_.pushConstantRangeCount);
        pipeline_layout_ = VkPipelineLayoutObj(layer_test_.DeviceObj(), {&descriptor_set_->layout_}, push_ranges);

        err = vkCreatePipelineCache(layer_test_.device(), &pc_ci_, NULL, &pipeline_cache_);
        ASSERT_VK_SUCCESS(err);
    }

    void LateBindPipelineInfo() {
        // By value or dynamically located items must be late bound
        gp_ci_.layout = pipeline_layout_.handle();
        gp_ci_.stageCount = shader_stages_.size();
        gp_ci_.pStages = shader_stages_.data();
        if ((gp_ci_.pTessellationState == nullptr) && IsValidVkStruct(tess_ci_)) {
            gp_ci_.pTessellationState = &tess_ci_;
        }
        if ((gp_ci_.pDynamicState == nullptr) && IsValidVkStruct(dyn_state_ci_)) {
            gp_ci_.pDynamicState = &dyn_state_ci_;
        }
    }

    VkResult CreateGraphicsPipeline(bool implicit_destroy = true, bool do_late_bind = true) {
        VkResult err;
        if (do_late_bind) {
            LateBindPipelineInfo();
        }
        if (implicit_destroy && (pipeline_ != VK_NULL_HANDLE)) {
            vkDestroyPipeline(layer_test_.device(), pipeline_, nullptr);
            pipeline_ = VK_NULL_HANDLE;
        }
        err = vkCreateGraphicsPipelines(layer_test_.device(), pipeline_cache_, 1, &gp_ci_, NULL, &pipeline_);
        return err;
    }

    // Helper function to create a simple test case (positive or negative)
    //
    // info_override can be any callable that takes a CreatePipelineHeper &
    // flags, error can be any args accepted by "SetDesiredFailure".
    template <typename Test, typename OverrideFunc, typename Error>
    static void OneshotTest(Test &test, OverrideFunc &info_override, const VkFlags flags, const std::vector<Error> &errors,
                            bool positive_test = false) {
        CreatePipelineHelper helper(test);
        helper.InitInfo();
        info_override(helper);
        helper.InitState();

        for (const auto &error : errors) test.Monitor()->SetDesiredFailureMsg(flags, error);
        helper.CreateGraphicsPipeline();

        if (positive_test) {
            test.Monitor()->VerifyNotFound();
        } else {
            test.Monitor()->VerifyFound();
        }
    }

    template <typename Test, typename OverrideFunc, typename Error>
    static void OneshotTest(Test &test, OverrideFunc &info_override, const VkFlags flags, Error error, bool positive_test = false) {
        OneshotTest(test, info_override, flags, std::vector<Error>(1, error), positive_test);
    }
};
namespace chain_util {
template <typename T>
T Init(const void *pnext_in = nullptr) {
    T pnext_obj = {};
    pnext_obj.sType = LvlTypeMap<T>::kSType;
    pnext_obj.pNext = pnext_in;
    return pnext_obj;
}
class ExtensionChain {
    const void *head_ = nullptr;
    typedef std::function<bool(const char *)> AddIfFunction;
    AddIfFunction add_if_;
    typedef std::vector<const char *> List;
    List *list_;

   public:
    template <typename F>
    ExtensionChain(F &add_if, List *list) : add_if_(add_if), list_(list) {}
    template <typename T>
    void Add(const char *name, T &obj) {
        if (add_if_(name)) {
            if (list_) {
                list_->push_back(name);
            }
            obj.pNext = head_;
            head_ = &obj;
        }
    }
    const void *Head() const { return head_; }
};
}  // namespace chain_util

// PushDescriptorProperties helper
VkPhysicalDevicePushDescriptorPropertiesKHR GetPushDescriptorProperties(VkInstance instance, VkPhysicalDevice gpu) {
    // Find address of extension call and make the call -- assumes needed extensions are enabled.
    PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR =
        (PFN_vkGetPhysicalDeviceProperties2KHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR");
    assert(vkGetPhysicalDeviceProperties2KHR != nullptr);

    // Get the push descriptor limits
    auto push_descriptor_prop = lvl_init_struct<VkPhysicalDevicePushDescriptorPropertiesKHR>();
    auto prop2 = lvl_init_struct<VkPhysicalDeviceProperties2KHR>(&push_descriptor_prop);
    vkGetPhysicalDeviceProperties2KHR(gpu, &prop2);
    return push_descriptor_prop;
}

#endif  // VKLAYERTEST_H
