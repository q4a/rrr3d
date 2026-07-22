#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <d3d9.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <vector>

namespace {

struct Vertex {
    float x;
    float y;
    float z;
    float rhw;
    std::uint32_t color;
};

constexpr DWORD kVertexFormat = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;
constexpr const char* kPortabilityEnumeration =
    "VK_KHR_portability_enumeration";
constexpr const char* kPortabilitySubset = "VK_KHR_portability_subset";

bool hasExtension(const std::vector<VkExtensionProperties>& extensions,
                  const char* name) {
    return std::any_of(extensions.begin(), extensions.end(),
                       [name](const VkExtensionProperties& extension) {
                           return std::strcmp(extension.extensionName, name) == 0;
                       });
}

void printVersion(std::uint32_t version) {
    std::printf("%u.%u.%u", VK_API_VERSION_MAJOR(version),
                VK_API_VERSION_MINOR(version), VK_API_VERSION_PATCH(version));
}

bool inspectVulkan() {
    if (!SDL_Vulkan_LoadLibrary(nullptr)) {
        std::fprintf(stderr, "SDL_Vulkan_LoadLibrary failed: %s\n",
                     SDL_GetError());
        return false;
    }

    std::uint32_t loaderVersion = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion(&loaderVersion) != VK_SUCCESS) {
        std::fprintf(stderr, "vkEnumerateInstanceVersion failed\n");
        SDL_Vulkan_UnloadLibrary();
        return false;
    }

    std::printf("Vulkan instance version: ");
    printVersion(loaderVersion);
    std::printf("\n");

    std::uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount,
                                           extensions.data());

    std::printf("Instance extensions (%u):\n", extensionCount);
    for (const auto& extension : extensions) {
        std::printf("  %s (%u)\n", extension.extensionName,
                    extension.specVersion);
    }

    std::uint32_t sdlExtensionCount = 0;
    const char* const* sdlExtensions =
        SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
    if (!sdlExtensions) {
        std::fprintf(stderr, "SDL_Vulkan_GetInstanceExtensions failed: %s\n",
                     SDL_GetError());
        SDL_Vulkan_UnloadLibrary();
        return false;
    }

    std::vector<const char*> enabledExtensions(
        sdlExtensions, sdlExtensions + sdlExtensionCount);
    const bool portabilityEnumeration =
        hasExtension(extensions, kPortabilityEnumeration);
    if (portabilityEnumeration) {
        enabledExtensions.push_back(kPortabilityEnumeration);
    }

    VkApplicationInfo application = {};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "rrr3d_dxvk_moltenvk_test";
    application.pEngineName = "RRR3D renderer spike";
    application.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.flags = portabilityEnumeration
                             ? VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
                             : 0;
    instanceInfo.pApplicationInfo = &application;
    instanceInfo.enabledExtensionCount =
        static_cast<std::uint32_t>(enabledExtensions.size());
    instanceInfo.ppEnabledExtensionNames = enabledExtensions.data();

    VkInstance instance = VK_NULL_HANDLE;
    const VkResult createResult =
        vkCreateInstance(&instanceInfo, nullptr, &instance);
    if (createResult != VK_SUCCESS) {
        std::fprintf(stderr, "vkCreateInstance failed: %d\n", createResult);
        SDL_Vulkan_UnloadLibrary();
        return false;
    }

    std::uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    std::printf("Physical devices (%u), portability enumeration: %s\n",
                deviceCount, portabilityEnumeration ? "enabled" : "not exposed");

    for (VkPhysicalDevice device : devices) {
        VkPhysicalDeviceProperties properties = {};
        VkPhysicalDeviceRobustness2FeaturesEXT robustness2 = {};
        robustness2.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
        VkPhysicalDeviceFeatures2 features = {};
        features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features.pNext = &robustness2;
        vkGetPhysicalDeviceProperties(device, &properties);
        vkGetPhysicalDeviceFeatures2(device, &features);

        std::uint32_t deviceExtensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr,
                                             &deviceExtensionCount, nullptr);
        std::vector<VkExtensionProperties> deviceExtensions(
            deviceExtensionCount);
        vkEnumerateDeviceExtensionProperties(
            device, nullptr, &deviceExtensionCount, deviceExtensions.data());

        std::printf("  %s, Vulkan ", properties.deviceName);
        printVersion(properties.apiVersion);
        std::printf(
            ", extensions=%u\n"
            "    VK_KHR_portability_subset: %s\n"
            "    geometryShader=%u shaderCullDistance=%u\n"
            "    VK_EXT_robustness2=%s "
            "robustBufferAccess2=%u nullDescriptor=%u\n"
            "    VK_KHR_pipeline_library=%s\n",
            deviceExtensionCount,
            hasExtension(deviceExtensions, kPortabilitySubset) ? "yes" : "no",
            features.features.geometryShader,
            features.features.shaderCullDistance,
            hasExtension(deviceExtensions, "VK_EXT_robustness2") ? "yes" : "no",
            robustness2.robustBufferAccess2, robustness2.nullDescriptor,
            hasExtension(deviceExtensions, "VK_KHR_pipeline_library") ? "yes"
                                                                       : "no");

        std::printf("    Device extensions:\n");
        for (const auto& extension : deviceExtensions) {
            std::printf("      %s (%u)\n", extension.extensionName,
                        extension.specVersion);
        }
    }

    vkDestroyInstance(instance, nullptr);
    SDL_Vulkan_UnloadLibrary();
    return deviceCount != 0;
}

int fail(const char* operation, HRESULT result = E_FAIL) {
    std::fprintf(stderr, "%s failed (HRESULT=0x%08x, SDL=%s)\n", operation,
                 static_cast<unsigned int>(result), SDL_GetError());
    return 1;
}

int runD3D9(SDL_Window* window) {
    const HWND nativeWindow = reinterpret_cast<HWND>(window);
    IDirect3D9* d3d = nullptr;

    try {
        d3d = Direct3DCreate9(D3D_SDK_VERSION);
    } catch (const std::exception& exception) {
        std::fprintf(stderr,
                     "DXVK Native rejected the Vulkan device: %s\n",
                     exception.what());
        return 2;
    } catch (...) {
        std::fprintf(stderr,
                     "DXVK Native rejected the Vulkan device with an "
                     "unknown exception\n");
        return 2;
    }

    if (!d3d) {
        return fail("Direct3DCreate9");
    }

    D3DPRESENT_PARAMETERS present = {};
    present.BackBufferWidth = 960;
    present.BackBufferHeight = 540;
    present.BackBufferFormat = D3DFMT_X8R8G8B8;
    present.BackBufferCount = 1;
    present.MultiSampleType = D3DMULTISAMPLE_NONE;
    present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present.hDeviceWindow = nativeWindow;
    present.Windowed = TRUE;
    present.EnableAutoDepthStencil = FALSE;
    present.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    IDirect3DDevice9* device = nullptr;
    HRESULT result = d3d->CreateDevice(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, nativeWindow,
        D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
        &present, &device);

    if (FAILED(result)) {
        d3d->Release();
        return fail("IDirect3D9::CreateDevice", result);
    }

    const std::array<Vertex, 3> vertices = {{
        {480.0F, 80.0F, 0.5F, 1.0F, D3DCOLOR_XRGB(255, 64, 64)},
        {780.0F, 440.0F, 0.5F, 1.0F, D3DCOLOR_XRGB(64, 255, 64)},
        {180.0F, 440.0F, 0.5F, 1.0F, D3DCOLOR_XRGB(64, 128, 255)},
    }};

    bool running = true;
    int frames = 0;
    while (running && frames < 180) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        result = device->Clear(0, nullptr, D3DCLEAR_TARGET,
                               D3DCOLOR_XRGB(24, 30, 44), 1.0F, 0);
        if (SUCCEEDED(result)) {
            result = device->BeginScene();
        }
        if (SUCCEEDED(result)) {
            result = device->SetFVF(kVertexFormat);
        }
        if (SUCCEEDED(result)) {
            result = device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1,
                                             vertices.data(), sizeof(Vertex));
        }
        if (SUCCEEDED(result)) {
            result = device->EndScene();
        }
        if (SUCCEEDED(result)) {
            result = device->Present(nullptr, nullptr, nativeWindow, nullptr);
        }

        if (FAILED(result)) {
            std::fprintf(stderr,
                         "D3D9 frame failed (HRESULT=0x%08x)\n",
                         static_cast<unsigned int>(result));
            break;
        }

        SDL_Delay(16);
        ++frames;
    }

    device->Release();
    d3d->Release();

    if (FAILED(result)) {
        return 1;
    }

    std::printf("Rendered %d D3D9 frames through DXVK Native.\n", frames);
    return 0;
}

}  // namespace

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return fail("SDL_Init");
    }

    SDL_Window* window = SDL_CreateWindow(
        "RRR3D DXVK + MoltenVK feasibility", 960, 540,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Quit();
        return fail("SDL_CreateWindow");
    }

    const bool vulkanReady = inspectVulkan();
    const int result = vulkanReady ? runD3D9(window) : 1;

    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
