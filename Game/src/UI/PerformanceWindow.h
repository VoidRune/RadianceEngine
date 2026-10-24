#pragma once
#include <RadianceEngine/Graphics/RenderGraph.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Game
{
	class GameApp;

	struct FrameTiming
	{
		float Interval = 0.0f;
		float Update = 0.0f;
		float Render = 0.0f;
		float Wait = 0.0f;
		float Sleep = 0.0f;
	};

	class PerformanceWindow
	{
	public:
		void Record(const FrameTiming& frame, const std::vector<Rdn::PassTiming>& passes);
		void Draw(GameApp& app, bool& open);

	private:
		struct Sample
		{
			float Interval = 0.0f;
			float Cpu = 0.0f;
			float Gpu = 0.0f;
		};

		struct Totals
		{
			double Interval = 0.0;
			double Update = 0.0;
			double Render = 0.0;
			double Wait = 0.0;
			double Sleep = 0.0;
			double Gpu = 0.0;
			double Busy = 0.0;
			float Worst = 0.0f;
			uint32_t Frames = 0;
			uint32_t GpuFrames = 0;
			std::vector<std::pair<std::string, double>> Passes;
		};

		void DrawSummary(GameApp& app);
		void DrawGraph(GameApp& app);
		void DrawPasses();
		void DrawGraphics(GameApp& app);
		void DrawSystem(GameApp& app);

		static constexpr size_t HistorySize = 300;
		std::array<Sample, HistorySize> m_History{};
		size_t m_HistoryNext = 0;
		size_t m_HistoryCount = 0;
		Totals m_Current;
		Totals m_Shown;
		Rdn::MemoryUsage m_Memory;
		bool m_Refreshed = true;
	};
}
