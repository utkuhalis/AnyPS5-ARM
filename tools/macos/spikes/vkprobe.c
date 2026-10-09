#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#define R(name, cond) printf("  %-48s %s\n", name, (cond) ? "YES" : "--- NO")
int main(int argc, char** argv){
  void* lib = dlopen(argv[1], RTLD_NOW); if(!lib){printf("dlopen fail %s\n", dlerror()); return 1;}
  PFN_vkGetInstanceProcAddr gipa = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vkGetInstanceProcAddr");
  PFN_vkCreateInstance ci = (PFN_vkCreateInstance)gipa(NULL, "vkCreateInstance");
  VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_3};
  const char* iext[] = {"VK_KHR_get_physical_device_properties2"};
  VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo=&app, .enabledExtensionCount=1, .ppEnabledExtensionNames=iext};
  VkInstance inst; VkResult r = ci(&ici, NULL, &inst); if (r){printf("vkCreateInstance %d\n", r); return 1;}
  #define F(n) PFN_##n n = (PFN_##n)gipa(inst, #n)
  F(vkEnumeratePhysicalDevices); F(vkGetPhysicalDeviceProperties2); F(vkGetPhysicalDeviceFeatures2); F(vkEnumerateDeviceExtensionProperties);
  uint32_t n=1; VkPhysicalDevice pd; vkEnumeratePhysicalDevices(inst,&n,&pd);
  VkPhysicalDeviceFloatControlsProperties fc={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES};
  VkPhysicalDeviceProperties2 p2={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,&fc}; vkGetPhysicalDeviceProperties2(pd,&p2);
  printf("GPU: %s  api %u.%u.%u\n", p2.properties.deviceName, VK_API_VERSION_MAJOR(p2.properties.apiVersion), VK_API_VERSION_MINOR(p2.properties.apiVersion), VK_API_VERSION_PATCH(p2.properties.apiVersion));
  uint32_t ec=0; vkEnumerateDeviceExtensionProperties(pd,NULL,&ec,NULL); VkExtensionProperties ex[512]; vkEnumerateDeviceExtensionProperties(pd,NULL,&ec,ex);
  #define HAS(e) ({int h=0; for(uint32_t i=0;i<ec;i++) if(!strcmp(ex[i].extensionName,e)) h=1; h;})
  VkPhysicalDeviceRobustness2FeaturesEXT rb={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
  VkPhysicalDevice8BitStorageFeatures b8={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES,&rb};
  VkPhysicalDeviceBufferDeviceAddressFeatures bda={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,&b8};
  VkPhysicalDeviceFeatures2 f2={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,&bda}; vkGetPhysicalDeviceFeatures2(pd,&f2);
  VkPhysicalDeviceFeatures* f=&f2.features;
  printf("HARD REQUIREMENTS:\n");
  R("VK_KHR_shader_float_controls", HAS("VK_KHR_shader_float_controls"));
  R("shaderSignedZeroInfNanPreserveFloat32", fc.shaderSignedZeroInfNanPreserveFloat32);
  R("VK_EXT_robustness2", HAS("VK_EXT_robustness2") || HAS("VK_KHR_robustness2"));
  R("nullDescriptor", rb.nullDescriptor);
  R("VK_KHR_8bit_storage + storageBuffer8BitAccess", HAS("VK_KHR_8bit_storage") && b8.storageBuffer8BitAccess);
  R("VK_KHR_buffer_device_address + bufferDeviceAddress", HAS("VK_KHR_buffer_device_address") && bda.bufferDeviceAddress);
  R("shaderInt64", f->shaderInt64);
  R("vertexPipelineStoresAndAtomics", f->vertexPipelineStoresAndAtomics);
  R("fragmentStoresAndAtomics", f->fragmentStoresAndAtomics);
  R("samplerAnisotropy", f->samplerAnisotropy);
  R("textureCompressionBC", f->textureCompressionBC);
  printf("OPTIONAL:\n");
  R("geometryShader", f->geometryShader); R("tessellationShader", f->tessellationShader);
  R("shaderFloat64", f->shaderFloat64); R("depthClamp", f->depthClamp); R("depthBounds", f->depthBounds);
  R("multiDrawIndirect", f->multiDrawIndirect); R("shaderStorageImageReadWithoutFormat", f->shaderStorageImageReadWithoutFormat);
  R("VK_EXT_external_memory_host", HAS("VK_EXT_external_memory_host"));
  R("VK_EXT_mesh_shader", HAS("VK_EXT_mesh_shader"));
  R("VK_KHR_timeline_semaphore", HAS("VK_KHR_timeline_semaphore"));
  R("VK_EXT_depth_clip_control", HAS("VK_EXT_depth_clip_control"));
  R("VK_KHR_fragment_shader_barycentric", HAS("VK_KHR_fragment_shader_barycentric"));
  R("VK_EXT_conservative_rasterization", HAS("VK_EXT_conservative_rasterization"));
  return 0;
}
