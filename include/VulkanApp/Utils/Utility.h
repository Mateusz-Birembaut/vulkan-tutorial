#ifndef UTILITY_H
#define UTILITY_H

#include <VulkanApp/Core/VulkanContext.h>

#include <cstdint>

inline constexpr unsigned int AlignTo(unsigned int operand, uint16_t alignment){
    return ((operand + (alignment - 1)) & ~(alignment - 1));
};

inline void setBufferName(VulkanContext* context, VkBuffer buffer, const char* name) {
	VkDebugUtilsObjectNameInfoEXT nameInfo{};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	nameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
	nameInfo.objectHandle = reinterpret_cast<uint64_t>(buffer);
	nameInfo.pObjectName = name;
	auto func = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(context->getDevice(), "vkSetDebugUtilsObjectNameEXT");
	if (func)
		func(context->getDevice(), &nameInfo);
}

/// @brief Creates a shader module and vector manages the alignment
/// @param code 
/// @return 
inline VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shader module!");
	}

	return shaderModule;
}

#endif // UTILITY_H