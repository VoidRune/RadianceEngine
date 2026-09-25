#pragma once
#include "Handle.h"
#include "Device.h"
#include "Resources/GpuBuffer.h"
#include "Resources/GpuRingBuffer.h"
#include "Resources/GpuImage.h"
#include "Resources/Sampler.h"
#include "Resources/Shader.h"
#include "Resources/TopLevelAS.h"
#include "Resources/Pipeline.h"
#include "Resources/RaytracingPipeline.h"
#include "Resources/DescriptorSet.h"
#include "CommandBuffer.h"

#include <string>
#include <unordered_set>
#include <unordered_map>

namespace Rdn
{
	class ResourceAllocator
	{
	public:
		static constexpr uint32_t MaxBindlessDescriptors = 1024;

		ResourceAllocator(Device* device);
		~ResourceAllocator();

		void* MapMemory(GpuBuffer* buffer);
		void UnmapMemory(GpuBuffer* buffer);
		void* MapMemory(GpuRingBuffer* ringBuffer, uint32_t elementIndex);
		void UnmapMemory(GpuRingBuffer* ringBuffer);
		uint64_t GetBufferDeviceAddress(GpuBuffer* buffer);
		void SetDeviceLocalBufferData(GpuBuffer* buffer, const void* data, uint32_t size);
		void SetImageData(GpuImage* image, const void* data, uint32_t size, ImageLayout newLayout);
		std::vector<uint8_t> GetImageData(GpuImage* image);

		void CreateGpuBuffer(GpuBuffer* gpuBuffer, const GpuBufferDesc& desc);
		void CreateGpuRingBuffer(GpuRingBuffer* gpuRingBuffer, const GpuRingBufferDesc& desc);
		void CreateGpuImage(GpuImage* gpuImage, const GpuImageDesc& desc);
		void CreateSampler(Sampler* sampler, const SamplerDesc& desc);
		void CreateShader(Shader* sampler, const ShaderDesc& desc);
		void CreateBottomLevelAS(BottomLevelAS* bottomLevelAS, const BottomLevelASDesc& desc);
		void CreateTopLevelAS(TopLevelAS* topLevelAS);
		void BuildTopLevelAS(TopLevelAS* topLevelAS);
		void CreatePipeline(Pipeline* pipeline, const PipelineDesc& desc);
		void CreateRaytracingPipeline(RayTracingPipeline* raytracingPipeline, const RayTracingPipelineDesc& desc);

		void AllocateDescriptorSet(DescriptorSet* descriptorSet, Pipeline* pipeline, uint32_t setIndex, uint32_t variableDescriptorCount = MaxBindlessDescriptors);
		void AllocateDescriptorSet(DescriptorSet* descriptorSet, RayTracingPipeline* pipeline, uint32_t setIndex, uint32_t variableDescriptorCount = MaxBindlessDescriptors);
		void UpdateDescriptorSet(DescriptorSet* descriptorSet, const DescriptorWrite& write);

		void ReleaseResource(GpuBuffer* gpuBuffer);
		void ReleaseResource(GpuRingBuffer* gpuRingBuffer);
		void ReleaseResource(GpuImage* gpuImage);
		void ReleaseResource(Sampler* sampler);
		void ReleaseResource(Shader* shader);
		void ReleaseResource(BottomLevelAS* bottomLevelAS);
		void ReleaseResource(TopLevelAS* topLevelAS);
		void ReleaseResource(Pipeline* pipeline);
		void ReleaseResource(RayTracingPipeline* raytracingPipeline);
		void ReleaseResource(DescriptorSet* descriptorSet);
		void FreeResources();

		std::string DescribeMemoryUsage() const;

		struct DescriptorSetLayoutKey
		{
			struct BindingKey {
				uint32_t Binding;
				uint32_t Count;
				DescriptorType Type;
				ShaderStage Stages;
				bool IsBindless;

				bool operator==(const BindingKey& o) const = default;
			};
			std::vector<BindingKey> Bindings;
			bool                    UsePushDescriptors = false;
			bool                    HasBindless = false;

			bool operator==(const DescriptorSetLayoutKey& o) const = default;
		};
		struct BindingKeyHasher {
			size_t operator()(const DescriptorSetLayoutKey::BindingKey& b) const noexcept {
				size_t seed = 0;
				auto mix = [&](size_t v) { seed ^= v + 0x9e3779b9ull + (seed << 6) + (seed >> 2); };
				mix(std::hash<uint32_t>{}(b.Binding));
				mix(std::hash<uint32_t>{}(b.Count));
				mix(std::hash<uint32_t>{}(static_cast<uint32_t>(b.Type)));
				mix(std::hash<uint32_t>{}(static_cast<uint32_t>(b.Stages)));
				mix(std::hash<bool>{}(b.IsBindless));
				return seed;
			}
		};

		struct DescriptorSetLayoutKeyHasher {
			size_t operator()(const DescriptorSetLayoutKey& k) const noexcept {
				size_t seed = 0;
				auto mix = [&](size_t v) { seed ^= v + 0x9e3779b9ull + (seed << 6) + (seed >> 2); };
				mix(std::hash<bool>{}(k.UsePushDescriptors));
				mix(std::hash<bool>{}(k.HasBindless));
				BindingKeyHasher bindingHasher;
				for (const auto& b : k.Bindings) {
					mix(bindingHasher(b));
				}
				return seed;
			}
		};
	private:

		struct DeviceLimits
		{
			uint64_t MinUniformBufferOffsetAlignment = 1;
			uint64_t MinStorageBufferOffsetAlignment = 1;
			uint64_t MinAccelerationStructureScratchOffsetAlignment = 1;
			uint32_t ShaderGroupHandleSize = 0;
			uint32_t ShaderGroupHandleAlignment = 1;
			uint32_t ShaderGroupBaseAlignment = 1;
		};

		Device* m_Device;
		DeviceHandle m_LogicalDevice;
		PhysicalDeviceHandle m_PhysicalDevice;
		AllocatorHandle m_Allocator;
		DescriptorPoolHandle m_DescriptorPool;
		DeviceLimits m_Limits;

		std::unordered_set<GpuBuffer*> m_Buffers;
		std::unordered_set<GpuRingBuffer*> m_RingBuffers;
		std::unordered_set<GpuImage*> m_Images;
		std::unordered_set<Sampler*> m_Samplers;
		std::unordered_set<Shader*> m_Shaders;
		std::unordered_set<BottomLevelAS*> m_BottomLevelASs;
		std::unordered_set<TopLevelAS*> m_TopLevelASs;
		std::unordered_set<Pipeline*> m_Pipelines;
		std::unordered_set<RayTracingPipeline*> m_RaytracingPipelines;
		std::unordered_set<DescriptorSet*> m_DescriptorSets;


		DescriptorSetLayoutHandle GetOrCreateDescriptorSetLayout(const DescriptorSetLayoutKey& key);
		void AllocateDescriptorSetFromLayout(DescriptorSet* descriptorSet, DescriptorSetLayoutHandle layout, uint32_t variableDescriptorCount);

		void DestroyGpuBuffer(GpuBuffer* gpuBuffer);
		void DestroyGpuRingBuffer(GpuRingBuffer* gpuRingBuffer);
		void DestroyGpuImage(GpuImage* gpuImage);
		void DestroySampler(Sampler* sampler);
		void DestroyShader(Shader* shader);
		void DestroyBottomLevelAS(BottomLevelAS* bottomLevelAS);
		void DestroyTopLevelAS(TopLevelAS* topLevelAS);
		void DestroyPipeline(Pipeline* pipeline);
		void DestroyRaytracingPipeline(RayTracingPipeline* raytracingPipeline);
		void DestroyDescriptorSet(DescriptorSet* descriptorSet);

		std::unordered_map<DescriptorSetLayoutKey, DescriptorSetLayoutHandle, DescriptorSetLayoutKeyHasher> m_DescriptorSetLayouts;
	};
}