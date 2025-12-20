#include <VulkanApp/Rendering/ComputePipeline.h>

#include <VulkanApp/Utils/FileReader.h>
#include <VulkanApp/Utils/Utility.h>

void ComputePipeline::init(VulkanContext* context, VkDescriptorSetLayout descriptorSetLayout) {
	m_context = context;
	m_descriptorSetLayout = descriptorSetLayout;
	createComputePipeline();
}

/// @brief destroys the pipeline then its layout
void ComputePipeline::cleanup() {
	if (m_pipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(m_context->getDevice(), m_pipeline, nullptr);
		m_pipeline = VK_NULL_HANDLE;
	}
	if (m_layout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(m_context->getDevice(), m_layout, nullptr);
		m_layout = VK_NULL_HANDLE;
	}
}

void ComputePipeline::createComputePipeline() {
	auto compShaderCode{FileReader::readSPV(compute_shader)};

	VkShaderModule compShaderModule{createShaderModule(m_context->getDevice(), compShaderCode)};

	VkPipelineShaderStageCreateInfo compShaderStageInfo{};
	compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	compShaderStageInfo.module = compShaderModule;
	compShaderStageInfo.pName = "main";

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;	  
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(m_context->getDevice(), &pipelineLayoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create compute pipeline layout!");
	} else {
		std::cout << "Compute pipeline layout created" << '\n';
	}

	VkComputePipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.stage = compShaderStageInfo;
	pipelineInfo.layout = m_layout;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; 
	pipelineInfo.basePipelineIndex = -1;		 

	if(vkCreateComputePipelines(m_context->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS){
		throw std::runtime_error("failed to create compute pipeline!");
	} else {
		std::cout << "Compute pipeline created" << '\n';
	}

	vkDestroyShaderModule(m_context->getDevice(), compShaderModule, nullptr);
}