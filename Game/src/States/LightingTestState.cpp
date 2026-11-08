#include "LightingTestState.h"
#include "App/GameApp.h"
#include "Rendering/WorldRenderer.h"
#include <RadianceEngine/Core/Log.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr float Fov = 40.0f;
		constexpr float SunIntensity = 2.36f;
		constexpr glm::vec3 SunDirection{ 0.45f, -1.0f, 0.3f };
		constexpr const char* Prefix = "LightingTest";

		glm::vec3 ToPathTracer(const glm::vec3& v)
		{
			return { -v.x, v.y, -v.z };
		}
	}

	LightingTestState::LightingTestState(GameApp& app)
		: m_App(app)
	{
		const WorldRenderer& renderer = app.GetWorldRenderer();
		const ModelLibrary& models = renderer.GetModels();
		for (uint32_t i = 0; i < uint32_t(models.GetData().Meshes.size()); i++)
			if (models.GetMesh(i).Name.starts_with(Prefix))
				m_Meshes.push_back(i);
		if (m_Meshes.empty())
			RDN_LOG_ERROR("The model library has no {}", Prefix);
		PostProcess& post = app.GetWorldRenderer().GetPost();
		post.Tonemap = Tonemapper::Aces;
		post.Exposure = 1.0f / TonemapperExposure[size_t(Tonemapper::Aces)];
		post.Grading = false;

		m_Lighting = { .SunDirection = SunDirection, .SunColor = glm::vec3(1.0f), .SunIntensity = SunIntensity, .SkyColor = glm::vec3(0.0f), .GroundColor = glm::vec3(0.0f) };
		const char* scene = "../../../Game/assets/Reference/LightingTest.gltf --env-intensity 0";
		const glm::vec3 eye = ToPathTracer(m_Eye);
		const glm::vec3 forward = glm::normalize(ToPathTracer(m_Target - m_Eye));
		const glm::vec3 sun = ToPathTracer(m_Lighting.SunDirection);
		RDN_LOG("Path tracer reference: ApplicationRelease.exe {} --size 1600x900 --bounces {}{} --camera {:.4f},{:.4f},{:.4f},{:.4f},{:.4f},{:.1f} --sun {:.4f},{:.4f},{:.4f},{:.4f}",
			scene, renderer.GetGiQuality() == GiQuality::Off ? 1 : 2, renderer.GetShadowQuality() == ShadowQuality::Off ? " --no-sun-shadows" : "",
			eye.x, eye.y, eye.z, glm::degrees(std::atan2(forward.z, forward.x)), glm::degrees(std::asin(forward.y)), Fov, sun.x, sun.y, sun.z, m_Lighting.SunIntensity);
	}

	bool LightingTestState::Render(Rdn::RenderGraph& graph, Rdn::RGImage target, Rdn::Extent2D extent, uint32_t frameIndex)
	{
		WorldRenderer& renderer = m_App.GetWorldRenderer();
		if (!renderer.IsReady())
			return false;
		const float aspect = float(std::max(extent.Width, 1u)) / float(std::max(extent.Height, 1u));
		glm::mat4 projection = glm::perspectiveLH_ZO(glm::radians(Fov), aspect, 0.1f, 100.0f);
		projection[1][1] *= -1.0f;
		const glm::mat4 view = glm::lookAtLH(m_Eye, m_Target, glm::vec3(0.0f, 1.0f, 0.0f));
		const WorldCamera camera{ .View = view, .Projection = projection, .ViewProjection = projection * view, .Eye = m_Eye, .Near = 0.1f, .Far = 100.0f };
		renderer.Begin(camera, m_Lighting, 0.0f, false);
		for (const uint32_t mesh : m_Meshes)
			renderer.DrawModel(mesh, glm::mat4(1.0f));
		renderer.Render(graph, target, extent, frameIndex, { 0.0f, 0.0f, 0.0f, 1.0f });
		return true;
	}
}
