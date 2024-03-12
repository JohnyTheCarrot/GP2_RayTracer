#include "RaytracingPipeline.h"
#include "Device.h"
#include "ShaderModule.h"
#include "src/utils/read_file.h"

namespace roing::vk {
	constexpr VkPushConstantRange PUSH_CONSTANT_RANGE{
	        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, 0,
	        sizeof(PushConstantRay)
	};

	RaytracingPipeline::RaytracingPipeline(const roing::vk::Device &device, VkDescriptorSetLayout hDescriptorSetLayout)
	    : m_hPipeline{VK_NULL_HANDLE, PipelineDestroyer{device.GetHandle()}}
	    , m_PipelineLayout{device, hDescriptorSetLayout, PUSH_CONSTANT_RANGE} {

		enum StageIndices {
			eRaygen,
			eMiss,
			eClosestHit,
			eShaderGroupCount,
		};

		std::array<VkPipelineShaderStageCreateInfo, eShaderGroupCount> stages{};
		VkPipelineShaderStageCreateInfo stageCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
		stageCreateInfo.pName = "main";

		auto         rgenShaderCode{utils::ReadFile("shaders/raytrace.rgen.spv")};
		ShaderModule raygenShaderModule{device, std::move(rgenShaderCode)};
		stageCreateInfo.module = raygenShaderModule.GetHandle();
		stageCreateInfo.stage  = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		stages[eRaygen]        = stageCreateInfo;

		auto         rmissShaderCode{utils::ReadFile("shaders/raytrace.rmiss.spv")};
		ShaderModule missShaderModule{device, std::move(rmissShaderCode)};
		stageCreateInfo.module = missShaderModule.GetHandle();
		stageCreateInfo.stage  = VK_SHADER_STAGE_MISS_BIT_KHR;
		stages[eMiss]          = stageCreateInfo;

		auto         rchitShaderCode{utils::ReadFile("shaders/raytrace.rchit.spv")};
		ShaderModule closestHitShaderModule{device, std::move(rchitShaderCode)};
		stageCreateInfo.module = closestHitShaderModule.GetHandle();
		stageCreateInfo.stage  = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		stages[eClosestHit]    = stageCreateInfo;

		// Shader groups
		VkRayTracingShaderGroupCreateInfoKHR groupCreateInfo{VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR
		};
		groupCreateInfo.anyHitShader       = VK_SHADER_UNUSED_KHR;
		groupCreateInfo.closestHitShader   = VK_SHADER_UNUSED_KHR;
		groupCreateInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

		groupCreateInfo.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		groupCreateInfo.generalShader = eRaygen;
		m_ShaderGroups.push_back(groupCreateInfo);

		groupCreateInfo.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		groupCreateInfo.generalShader = eMiss;
		m_ShaderGroups.push_back(groupCreateInfo);

		groupCreateInfo.type             = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
		groupCreateInfo.generalShader    = VK_SHADER_UNUSED_KHR;
		groupCreateInfo.closestHitShader = eClosestHit;
		m_ShaderGroups.push_back(groupCreateInfo);


		VkRayTracingPipelineCreateInfoKHR rayPipelineInfo{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
		rayPipelineInfo.stageCount = stages.size();
		rayPipelineInfo.pStages    = stages.data();

		rayPipelineInfo.groupCount = m_ShaderGroups.size();
		rayPipelineInfo.pGroups    = m_ShaderGroups.data();

		rayPipelineInfo.maxPipelineRayRecursionDepth = 2;// ray depth
		rayPipelineInfo.layout                       = m_PipelineLayout.GetHandle();

		VkPipeline hPipeline;
		roing::vk::vkCreateRayTracingPipelinesKHR(device.GetHandle(), {}, {}, 1, &rayPipelineInfo, nullptr, &hPipeline);
		m_hPipeline.reset(hPipeline);
	}

	PipelineHandle::pointer RaytracingPipeline::GetHandle() const noexcept {
		return m_hPipeline.get();
	}
}// namespace roing::vk
