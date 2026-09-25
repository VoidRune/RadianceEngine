#pragma once
#include "RendererBase.h"
#include <RadianceEngine/Window/Window.h>
#include <RadianceEngine/Graphics/Device.h>
#include <RadianceEngine/Graphics/PresentQueue.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <RadianceEngine/Graphics/RenderGraph.h>
#include <RadianceEngine/Graphics/Resources/TopLevelAS.h>
#include "Core/CameraFP.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdint>
#include <functional>

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 uv;

	bool operator==(const Vertex& other) const noexcept {
		return pos == other.pos && normal == other.normal && uv == other.uv;
	}
};

struct VertexHasher {
	size_t operator()(const Vertex& v) const noexcept {
		size_t h1 = std::hash<float>{}(v.pos.x) ^ (std::hash<float>{}(v.pos.y) << 1) ^ (std::hash<float>{}(v.pos.z) << 2);
		size_t h2 = std::hash<float>{}(v.normal.x) ^ (std::hash<float>{}(v.normal.y) << 1) ^ (std::hash<float>{}(v.normal.z) << 2);
		size_t h3 = std::hash<float>{}(v.uv.x) ^ (std::hash<float>{}(v.uv.y) << 1);
		return h1 ^ (h2 << 1) ^ (h3 << 2);
	}
};

struct Material
{
	glm::vec4 Color = glm::vec4(1);
	glm::vec4 Emission = glm::vec4(0);
	float Metallic = 0.0f;
	float Roughness = 0.0f;
	float Transmission = 0.0f;
	uint32_t TextureIndex = 0;

	bool operator==(const Material& other) const noexcept
	{
		return Color == other.Color &&
			Emission == other.Emission &&
			Metallic == other.Metallic &&
			Roughness == other.Roughness &&
			Transmission == other.Transmission &&
			TextureIndex == other.TextureIndex;
	}
};

struct MeshPrimitive
{
	uint64_t VertexBufferDeviceAddress;
	uint64_t IndexBufferDeviceAddress;
	uint32_t MaterialIndex = 0;
};

struct Model
{
	Rdn::GpuBuffer VertexBuffer;
	Rdn::GpuBuffer IndexBuffer;
	Rdn::BottomLevelAS BottomLevelAS;
	uint64_t VertexBufferDeviceAddress = 0;
	uint64_t IndexBufferDeviceAddress = 0;
};

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

	void CreateAccelerationStructure();
	void CreatePipelines();
	void CreateSamplers();
	void CreateImages();

	bool LoadObjModel(std::string filePath, std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices);
	void LoadModel(std::string filepath, Model* model);
	
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
	} globalFrameData;
	
	void AddInstance(std::vector<MeshPrimitive>& meshInfos, std::vector<Material>& materials, Model* model, glm::mat4 transform, const Material& material);

	std::unique_ptr<Rdn::GpuBuffer> m_MeshInfoBuffer;
	std::unique_ptr<Rdn::GpuBuffer> m_MaterialBuffer;
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
	
	std::unique_ptr<Model> m_Plane;
	std::unique_ptr<Model> m_Dragon;
	std::unique_ptr<Model> m_Sphere;
	std::unique_ptr<Rdn::TopLevelAS> m_Scene;
	std::unique_ptr<Rdn::GpuImage> m_WhiteTexture;
	std::unique_ptr<Rdn::GpuImage> m_Texture;

	std::unique_ptr<Rdn::Sampler> m_NearestSampler;
	std::unique_ptr<Rdn::Sampler> m_LinearSampler;
	std::unique_ptr<Rdn::Shader> m_CompositeVertShader;
	std::unique_ptr<Rdn::Shader> m_CompositeFragShader;
	std::unique_ptr<Rdn::Pipeline> m_CompositePipeline;

};