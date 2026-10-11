#pragma once
#include "App/AppState.h"
#include "Rendering/Lighting.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace Game
{
	class GameApp;

	class LightingTestState final : public AppState
	{
	public:
		LightingTestState(GameApp& app, const std::string& model = {});

		bool Render(Rdn::RenderGraph& graph, Rdn::RGImage target, Rdn::Extent2D extent, uint32_t frameIndex) override;

	private:
		GameApp& m_App;
		std::vector<uint32_t> m_Meshes;
		glm::vec3 m_Eye{ 0.0f, 4.9f, -3.0f };
		glm::vec3 m_Target{ 0.0f, 0.3f, 0.0f };
		SceneLighting m_Lighting;
	};
}
