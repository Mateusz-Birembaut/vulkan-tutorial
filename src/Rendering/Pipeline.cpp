#include <VulkanApp/Rendering/Pipeline.h>

#include <VulkanApp/Utils/FileReader.h>
#include <VulkanApp/Core/VulkanContext.h>
#include <VulkanApp/Resources/Mesh.h>

#include <vulkan/vulkan.h>
#include <stdexcept>
#include <iostream>

/// @brief Creates a shader module and vector manages the alignment
/// @param code 
/// @return 
VkShaderModule Pipeline::createShaderModule(const std::vector<char>& code) {
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(m_context->getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shader module!");
	}

	return shaderModule;
}


void Pipeline::init(VulkanContext* context, VkRenderPass renderPass, VkDescriptorSetLayout descriptorSetLayout) {
	m_context = context;
	m_renderPass = renderPass;
	m_descriptorSetLayout = descriptorSetLayout;
	createGraphicsPipeline();
}

/// @brief destroys the pipeline then its layout
void Pipeline::cleanup() {
	if (m_pipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(m_context->getDevice(), m_pipeline, nullptr);
		m_pipeline = VK_NULL_HANDLE;
	}
	if (m_layout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(m_context->getDevice(), m_layout, nullptr);
		m_layout = VK_NULL_HANDLE;
	}
}

/**
 * @brief Creates the graphics pipeline used for rendering.
 *
 * Implementation summary:
 * - Loads SPIR-V vertex and fragment shaders and creates shader modules.
 * - Configures vertex input (binding + attribute descriptions) from `Vertex`.
 * - Sets input assembly to triangle list.
 * - Uses dynamic viewport and scissor state so they can be set at command recording.
 * - Configures rasterizer (back-face culling, fill polygon mode, CCW front face).
 * - Enables MSAA using sample count provided by the `VulkanContext`.
 * - Sets up depth/stencil state (depth test/write enabled, compare = LESS).
 * - Configures color blending with blending disabled (simple replace).
 * - Creates a pipeline layout that binds the provided descriptor set layout.
 * - Finally creates the graphics pipeline for `m_renderPass` subpass 0.
 */
void Pipeline::createGraphicsPipeline() {

	auto vertShaderCode{FileReader::readSPV(g_vertex_shader)};
	auto fragShaderCode{FileReader::readSPV(g_fragment_shader)};

	VkShaderModule vertShaderModule{createShaderModule(vertShaderCode)};
	VkShaderModule fragShaderModule{createShaderModule(fragShaderCode)};

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT; // where we'll use the shader

	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	// vertShaderStageInfo.pSpecializationInfo to define constants => better optimisations

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[]{vertShaderStageInfo, fragShaderStageInfo};

	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	std::vector<VkDynamicState> dynamicStates{
	    VK_DYNAMIC_STATE_VIEWPORT,
	    VK_DYNAMIC_STATE_SCISSOR,
	}; 

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPipelineViewportStateCreateInfo viewportInfo{};
	viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportInfo.scissorCount = 1;
	viewportInfo.viewportCount = 1;

	// Depth testing, face culling, scissor test and wireframe mode
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f; 
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;	
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;	
	rasterizer.depthBiasSlopeFactor = 0.0f;	 

	// MSAA
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_TRUE;
	multisampling.rasterizationSamples = m_context->getMsaaSamples();
	multisampling.minSampleShading = 0.2f;

	// Depth / stencil testing
	// see VkPipelineDepthStencilStateCreateInfo

	// Color blending

	// VkPipelineColorBlendAttachmentState contains the configuration per attached framebuffer and the second struct
	// VkPipelineColorBlendStateCreateInfo contains the global color blending settings

	// Config 
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE; // on blend pas
	// notre nouvelle image ne sera pas combiné avec l'ancienne
	// le plus commun c'est de blend en utilisant l'alpha channel

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f; 
	colorBlending.blendConstants[1] = 0.0f; 
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	// Pipeline layout, uniforms ect.
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;	  
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(m_context->getDevice(), &pipelineLayoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	} else {
		std::cout << "Pipeline layout created" << '\n';
	}

	VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
	depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilInfo.depthTestEnable = VK_TRUE;
	depthStencilInfo.depthWriteEnable = VK_TRUE;
	depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilInfo.minDepthBounds = 0.0f;		 
	depthStencilInfo.maxDepthBounds = 1.0f;		
	depthStencilInfo.stencilTestEnable = VK_FALSE;
	depthStencilInfo.front = {};
	depthStencilInfo.back = {};

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;

	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportInfo;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.pDepthStencilState = &depthStencilInfo;
	pipelineInfo.layout = m_layout;
	pipelineInfo.renderPass = m_renderPass;
	pipelineInfo.subpass = 0; 
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; 
	pipelineInfo.basePipelineIndex = -1;		 

	if (vkCreateGraphicsPipelines(m_context->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create graphics pipeline!");
	} else {
		std::cout << "Graphics pipeline created" << '\n';
	}

	vkDestroyShaderModule(m_context->getDevice(), vertShaderModule, nullptr);
	vkDestroyShaderModule(m_context->getDevice(), fragShaderModule, nullptr);
}