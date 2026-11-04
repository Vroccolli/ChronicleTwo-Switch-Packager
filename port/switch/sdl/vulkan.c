// ChronicleTwo's libnx/Vulkan extension to the pinned SDL3 Switch backend.
#include "SDL_internal.h"
#define VK_USE_PLATFORM_VI_NN
#include "video/SDL_vulkan_internal.h"
#include "video/SDL_sysvideo.h"
#include <switch.h>

extern PFN_vkVoidFunction vkGetInstanceProcAddr(VkInstance instance, const char *name);
static bool Load(SDL_VideoDevice *video, const char *path) {
    if (path && *path) return SDL_SetError("Switch Vulkan is linked statically; a loader path is not supported");
    SDL_setenv_unsafe("NVK_I_WANT_A_BROKEN_VULKAN_DRIVER", "1", 1);
    video->vulkan_config.loader_handle = (SDL_SharedObject *)video;
    video->vulkan_config.vkGetInstanceProcAddr = (SDL_FunctionPointer)vkGetInstanceProcAddr;
    video->vulkan_config.vkEnumerateInstanceExtensionProperties =
        (SDL_FunctionPointer)vkGetInstanceProcAddr(VK_NULL_HANDLE,"vkEnumerateInstanceExtensionProperties");
    return video->vulkan_config.vkEnumerateInstanceExtensionProperties != NULL;
}
static void Unload(SDL_VideoDevice *video) {
    video->vulkan_config.loader_handle = NULL;
    video->vulkan_config.vkGetInstanceProcAddr = NULL;
    video->vulkan_config.vkEnumerateInstanceExtensionProperties = NULL;
}
static const char *const *Extensions(SDL_VideoDevice *video, Uint32 *count) {
    (void)video;
    static const char *names[] = {VK_KHR_SURFACE_EXTENSION_NAME,VK_NN_VI_SURFACE_EXTENSION_NAME};
    if (count) *count = SDL_arraysize(names);
    return names;
}
static bool Create(SDL_VideoDevice *video, SDL_Window *window, VkInstance instance,
                   const VkAllocationCallbacks *allocator, VkSurfaceKHR *surface) {
    (void)video; (void)window;
    PFN_vkCreateViSurfaceNN create = (PFN_vkCreateViSurfaceNN)vkGetInstanceProcAddr(instance,"vkCreateViSurfaceNN");
    if (!create) return SDL_SetError("VK_NN_vi_surface is unavailable");
    VkViSurfaceCreateInfoNN info = {0}; info.sType=VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN;
    info.window = nwindowGetDefault();
    VkResult result = create(instance,&info,allocator,surface);
    return result == VK_SUCCESS ? true : SDL_SetError("vkCreateViSurfaceNN: %d",result);
}
static void Destroy(SDL_VideoDevice *video, VkInstance instance, VkSurfaceKHR surface,
                    const VkAllocationCallbacks *allocator) {
    (void)video;
    PFN_vkDestroySurfaceKHR destroy = (PFN_vkDestroySurfaceKHR)vkGetInstanceProcAddr(instance,"vkDestroySurfaceKHR");
    if (destroy && surface) destroy(instance,surface,allocator);
}
static bool PresentSupport(SDL_VideoDevice *video, VkInstance instance, VkPhysicalDevice device, Uint32 family) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!Create(video,NULL,instance,NULL,&surface)) return false;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR query =
        (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceSurfaceSupportKHR");
    VkBool32 supported = VK_FALSE;
    VkResult result = query ? query(device,family,surface,&supported) : VK_ERROR_EXTENSION_NOT_PRESENT;
    Destroy(video,instance,surface,NULL);
    return result == VK_SUCCESS && supported;
}
void ctSdlAttachVulkan(SDL_VideoDevice *video) {
    video->Vulkan_LoadLibrary = Load;
    video->Vulkan_UnloadLibrary = Unload;
    video->Vulkan_GetInstanceExtensions = Extensions;
    video->Vulkan_CreateSurface = Create;
    video->Vulkan_DestroySurface = Destroy;
    video->Vulkan_GetPresentationSupport = PresentSupport;
}
