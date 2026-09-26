#pragma once
#include "RendererBase.h"
#include <RadianceEngine/Window/Window.h>
#include <RadianceEngine/Graphics/Device.h>
#include <RadianceEngine/Graphics/PresentQueue.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <RadianceEngine/Graphics/RenderGraph.h>
#include "Core/CameraFP.h"
#include "Scene.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdint>

class PathTracer : public RendererBase
{
public:
	PathTracer(Rdn::Window* window, Rdn::Device* device, Rdn::PresentQueue* presentQueue, Rdn::ResourceAllocator* resourceAllocator, Rdn::RenderGraph* renderGraph);
	~PathTracer();

	void RenderFrame(float elapsedTime);
	void SwapchainResized();
	void RecompileShaders();
	void WaitForFrameEnd();

private:

	void CreateScene();
	void CreateSceneDescriptorSet();
	bool CreatePipelines();
	void CreateSamplers();
	void CreateImages();

	
	Rdn::Window* m_Window;
	Rdn::Device* m_Device;
	Rdn::PresentQueue* m_PresentQueue;
	Rdn::ResourceAllocator* m_ResourceAllocator;
	Rdn::RenderGraph* m_RenderGraph;

	struct GlobalFrameData
	{
		glm::vec4 CameraPosition;
		glm::vec4 CameraForward;
		glm::vec4 CameraRight;
		glm::vec4 CameraUp;
		float TanHalfFov = 0;
		float AspectRatio = 0;
		float Aperture = 0;
		float FocusDistance = 0;
		uint32_t FrameIndex = 0;
		uint32_t MaxBounces = 64;
		float Exposure = 1.0f;
		uint32_t SamplesPerPixel = 1;
	} globalFrameData;
	
	std::unique_ptr<Scene> m_Scene;
	std::unique_ptr<Rdn::DescriptorSet> m_SceneDescriptorSet;
	std::unique_ptr<Rdn::GpuRingBuffer> m_GlobalDataBuffer;
	std::unique_ptr<CameraFP> m_Camera;
	bool m_IsEvenFrame = false;
	bool m_ClearAccumulation = true;
	bool m_LogRenderGraph = false;

	std::unique_ptr<Rdn::Shader> m_RayGenShader;
	std::unique_ptr<Rdn::Shader> m_RayMissShader;
	std::unique_ptr<Rdn::Shader> m_RayClosestHitShader;
	std::unique_ptr<Rdn::RayTracingPipeline> m_RayTracingPipeline;
	std::unique_ptr<Rdn::GpuImage> m_AccumulationImage1;
	std::unique_ptr<Rdn::GpuImage> m_AccumulationImage2;
	std::unique_ptr<Rdn::GpuImage> m_OutputImage;
	

	std::unique_ptr<Rdn::Sampler> m_NearestSampler;
	std::unique_ptr<Rdn::Sampler> m_LinearSampler;
	std::unique_ptr<Rdn::Shader> m_CompositeVertShader;
	std::unique_ptr<Rdn::Shader> m_CompositeFragShader;
	std::unique_ptr<Rdn::Pipeline> m_CompositePipeline;

	std::vector<Rdn::ShaderSourceFile> m_ShaderSources;
	float m_NextShaderCheckTime = 0.0f;
};