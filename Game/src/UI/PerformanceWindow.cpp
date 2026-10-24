#include "PerformanceWindow.h"
#include "App/GameApp.h"
#include "UI/Ui.h"
#include <imgui.h>
#include <algorithm>
#include <cfloat>
#include <cstdio>

namespace Game
{
	namespace
	{
		constexpr double SummaryMilliseconds = 500.0;
		constexpr float GraphWidth = 470.0f;
		constexpr float GraphHeight = 96.0f;
		constexpr ImVec4 FrameColor(0.86f, 0.86f, 0.92f, 1.0f);
		constexpr ImVec4 CpuColor(1.0f, 0.68f, 0.32f, 1.0f);
		constexpr ImVec4 GpuColor(0.42f, 0.84f, 1.0f, 1.0f);
		constexpr ImVec4 CapColor(0.95f, 0.45f, 0.55f, 1.0f);

		double Gigabytes(uint64_t bytes)
		{
			return double(bytes) / double(1ull << 30);
		}

		const char* PresentModeName(Rdn::PresentMode mode)
		{
			switch (mode)
			{
			case Rdn::PresentMode::Immediate:
				return "immediate, may tear";
			case Rdn::PresentMode::Mailbox:
				return "mailbox, no vsync wait";
			case Rdn::PresentMode::Fifo:
				return "FIFO, vsync";
			case Rdn::PresentMode::FifoRelaxed:
				return "relaxed FIFO, vsync";
			}
			return "unknown";
		}

		const char* Limit(double interval, double cpu, double gpu, double sleep, bool vsync)
		{
			if (sleep > interval * 0.1)
				return "the frame cap";
			if (vsync && cpu < interval * 0.85 && gpu < interval * 0.85)
				return "vsync";
			return gpu >= cpu ? "the GPU" : "the CPU";
		}

		std::string GroupKey(const std::string& name)
		{
			std::string key = name.substr(0, name.find(' '));
			if (key.size() > 1 && key.back() == 's')
				key.pop_back();
			return key;
		}

		void ShareBar(double fraction)
		{
			constexpr float Width = 84.0f;
			const float height = ImGui::GetTextLineHeight();
			const ImVec2 origin = ImGui::GetCursorScreenPos();
			ImGui::Dummy(ImVec2(Width, height));
			ImDrawList* list = ImGui::GetWindowDrawList();
			const float top = origin.y + height * 0.3f;
			const float bottom = origin.y + height * 0.7f;
			list->AddRectFilled(ImVec2(origin.x, top), ImVec2(origin.x + Width, bottom), IM_COL32(255, 255, 255, 24), 2.0f);
			list->AddRectFilled(ImVec2(origin.x, top), ImVec2(origin.x + Width * float(std::clamp(fraction, 0.0, 1.0)), bottom), ImGui::GetColorU32(GpuColor), 2.0f);
			ImGui::SameLine();
			ImGui::Text("%3.0f%%", fraction * 100.0);
		}
	}

	void PerformanceWindow::Record(const FrameTiming& frame, const std::vector<Rdn::PassTiming>& passes)
	{
		float gpu = 0.0f;
		float busy = 0.0f;
		std::vector<std::pair<std::string, double>>& totals = m_Current.Passes;
		for (size_t i = 0; i < passes.size(); i++)
		{
			const Rdn::PassTiming& pass = passes[i];
			gpu = std::max(gpu, pass.Start + pass.Milliseconds);
			busy += pass.Milliseconds;
			if (i < totals.size() && totals[i].first == pass.Name)
			{
				totals[i].second += pass.Milliseconds;
				continue;
			}
			const auto it = std::find_if(totals.begin(), totals.end(), [&](const auto& entry) { return entry.first == pass.Name; });
			if (it != totals.end())
				it->second += pass.Milliseconds;
			else
				totals.emplace(totals.begin() + std::min(i, totals.size()), pass.Name, pass.Milliseconds);
		}
		m_History[m_HistoryNext] = { frame.Interval, frame.Update + frame.Render, gpu };
		m_HistoryNext = (m_HistoryNext + 1) % HistorySize;
		m_HistoryCount = std::min(m_HistoryCount + 1, HistorySize);

		m_Current.Interval += frame.Interval;
		m_Current.Update += frame.Update;
		m_Current.Render += frame.Render;
		m_Current.Wait += frame.Wait;
		m_Current.Sleep += frame.Sleep;
		m_Current.Worst = std::max(m_Current.Worst, frame.Interval);
		m_Current.Frames++;
		if (!passes.empty())
		{
			m_Current.Gpu += gpu;
			m_Current.Busy += busy;
			m_Current.GpuFrames++;
		}
		if (m_Current.Interval >= SummaryMilliseconds)
		{
			m_Shown = std::move(m_Current);
			m_Current = {};
			m_Refreshed = true;
		}
	}

	void PerformanceWindow::Draw(GameApp& app, bool& open)
	{
		if (m_Refreshed)
		{
			m_Memory = app.GetMemoryUsage();
			m_Refreshed = false;
		}
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 16.0f, viewport->WorkPos.y + 16.0f), ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
		ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, viewport->WorkSize.y - 32.0f));
		ImGui::SetNextWindowBgAlpha(0.9f);
		if (!ImGui::Begin("Performance (F2)", &open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::End();
			return;
		}
		DrawSummary(app);
		DrawGraph(app);
		if (ImGui::BeginTabBar("performance.tabs"))
		{
			if (ImGui::BeginTabItem("GPU passes"))
			{
				DrawPasses();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Graphics"))
			{
				DrawGraphics(app);
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("System"))
			{
				DrawSystem(app);
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::End();
	}

	void PerformanceWindow::DrawSummary(GameApp& app)
	{
		const Totals& shown = m_Shown;
		if (shown.Frames == 0)
		{
			ImGui::TextDisabled("Measuring...");
			return;
		}
		const double frames = double(shown.Frames);
		const double interval = shown.Interval / frames;
		const double update = shown.Update / frames;
		const double render = shown.Render / frames;
		const double cpu = update + render;
		const double sleep = shown.Sleep / frames;
		ImGui::Text("%.0f fps, %.2f ms a frame, worst %.2f ms", 1000.0 / interval, interval, double(shown.Worst));
		ImGui::TextColored(CpuColor, "CPU %.2f ms", cpu);
		ImGui::SameLine();
		ImGui::TextDisabled("game and UI %.2f, render setup %.2f", update, render);
		double gpu = 0.0;
		if (shown.GpuFrames > 0)
		{
			gpu = shown.Gpu / double(shown.GpuFrames);
			ImGui::TextColored(GpuColor, "GPU %.2f ms", gpu);
			ImGui::SameLine();
			ImGui::TextDisabled("%.2f of it in passes", shown.Busy / double(shown.GpuFrames));
		}
		else
			ImGui::TextDisabled("GPU timings aren't available on this device");
		ImGui::Text("Waiting: GPU and display %.2f ms, frame cap %.2f ms", shown.Wait / frames, sleep);
		ImGui::Text("Limited by %s", Limit(interval, cpu, gpu, sleep, app.GetSettings().Vsync));
	}

	void PerformanceWindow::DrawGraph(GameApp& app)
	{
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		ImGui::Dummy(ImVec2(GraphWidth, GraphHeight));
		const ImVec2 end(origin.x + GraphWidth, origin.y + GraphHeight);
		ImDrawList* list = ImGui::GetWindowDrawList();
		list->AddRectFilled(origin, end, IM_COL32(6, 6, 14, 230), 4.0f);

		float peak = 0.0f;
		for (size_t i = 0; i < m_HistoryCount; i++)
			peak = std::max({ peak, m_History[i].Interval, m_History[i].Cpu, m_History[i].Gpu });
		float top = 1000.0f / 30.0f;
		for (const float step : { 50.0f, 100.0f, 250.0f })
			if (peak > top)
				top = step;
		const auto height = [&](float ms) { return end.y - 1.0f - std::min(ms / top, 1.0f) * (GraphHeight - 2.0f); };

		float labelled = origin.y;
		for (const int fps : { 10, 15, 20, 30, 60, 120, 240 })
		{
			const float ms = 1000.0f / float(fps);
			const float y = height(ms);
			if (ms >= top || y - labelled < ImGui::GetTextLineHeight() || end.y - y < 4.0f)
				continue;
			labelled = y;
			list->AddLine(ImVec2(origin.x, y), ImVec2(end.x, y), IM_COL32(255, 255, 255, 36));
			char label[16];
			std::snprintf(label, sizeof(label), "%d fps", fps);
			list->AddText(ImVec2(origin.x + 4.0f, y - ImGui::GetTextLineHeight()), IM_COL32(255, 255, 255, 90), label);
		}
		const Settings& settings = app.GetSettings();
		if (!settings.Vsync && 1000.0f / float(settings.FrameCap) < top)
		{
			const float y = height(1000.0f / float(settings.FrameCap));
			list->AddLine(ImVec2(origin.x, y), ImVec2(end.x, y), ImGui::GetColorU32(ImVec4(CapColor.x, CapColor.y, CapColor.z, 0.45f)));
		}

		if (m_HistoryCount > 1)
		{
			const float step = GraphWidth / float(HistorySize - 1);
			std::array<ImVec2, HistorySize> points;
			const auto plot = [&](float Sample::* field, const ImVec4& color)
			{
				for (size_t i = 0; i < m_HistoryCount; i++)
				{
					const Sample& sample = m_History[(m_HistoryNext + HistorySize - m_HistoryCount + i) % HistorySize];
					points[i] = ImVec2(end.x - float(m_HistoryCount - 1 - i) * step, height(sample.*field));
				}
				list->AddPolyline(points.data(), int(m_HistoryCount), ImGui::GetColorU32(color), ImDrawFlags_None, 1.5f);
			};
			plot(&Sample::Interval, FrameColor);
			plot(&Sample::Cpu, CpuColor);
			plot(&Sample::Gpu, GpuColor);
		}

		ImGui::TextColored(FrameColor, "Frame");
		ImGui::SameLine();
		ImGui::TextColored(CpuColor, "CPU");
		ImGui::SameLine();
		ImGui::TextColored(GpuColor, "GPU");
		if (!settings.Vsync)
		{
			ImGui::SameLine();
			ImGui::TextColored(CapColor, "Cap");
		}
		ImGui::SameLine();
		ImGui::TextDisabled("last %zu frames, top %.0f ms", m_HistoryCount, double(top));
	}

	void PerformanceWindow::DrawPasses()
	{
		const Totals& shown = m_Shown;
		if (shown.GpuFrames == 0 || shown.Passes.empty())
		{
			ImGui::TextDisabled("Waiting for GPU timings");
			return;
		}

		struct Group
		{
			std::string Key;
			std::vector<size_t> Members;
			double Total = 0.0;
		};
		std::vector<Group> groups;
		for (size_t i = 0; i < shown.Passes.size(); i++)
		{
			std::string key = GroupKey(shown.Passes[i].first);
			auto it = std::find_if(groups.begin(), groups.end(), [&](const Group& group) { return group.Key == key; });
			if (it == groups.end())
				it = groups.insert(groups.end(), Group{ std::move(key) });
			it->Members.push_back(i);
			it->Total += shown.Passes[i].second;
		}

		const double frames = double(shown.GpuFrames);
		const double busy = std::max(shown.Busy / frames, 1e-6);
		if (!ImGui::BeginTable("performance.passes", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
			return;
		ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthFixed, 250.0f);
		ImGui::TableSetupColumn("ms", ImGuiTableColumnFlags_WidthFixed, 56.0f);
		ImGui::TableSetupColumn("Share", ImGuiTableColumnFlags_WidthFixed, 140.0f);
		ImGui::TableHeadersRow();
		const auto values = [&](double total)
		{
			ImGui::TableNextColumn();
			ImGui::Text("%.2f", total / frames);
			ImGui::TableNextColumn();
			ShareBar(total / frames / busy);
		};
		constexpr ImGuiTreeNodeFlags Leaf = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth;
		for (const Group& group : groups)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			const std::string& first = shown.Passes[group.Members.front()].first;
			if (group.Members.size() == 1)
			{
				ImGui::TreeNodeEx(first.c_str(), Leaf);
				values(group.Total);
				continue;
			}
			const bool expanded = ImGui::TreeNodeEx(group.Key.c_str(), ImGuiTreeNodeFlags_SpanFullWidth, "%s (%zu passes)", first.substr(0, first.find(' ')).c_str(), group.Members.size());
			values(group.Total);
			if (!expanded)
				continue;
			for (const size_t index : group.Members)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TreeNodeEx(shown.Passes[index].first.c_str(), Leaf | ImGuiTreeNodeFlags_Bullet);
				values(shown.Passes[index].second);
			}
			ImGui::TreePop();
		}
		ImGui::EndTable();
		ImGui::TextDisabled("Averaged over half a second");
	}

	void PerformanceWindow::DrawGraphics(GameApp& app)
	{
		const Settings& settings = app.GetSettings();
		ImGui::Text("Indirect light %s, shadows %s", GiQualityNames[size_t(settings.Gi)], ShadowQualityNames[size_t(settings.Shadows)]);
		if (settings.Vsync)
			ImGui::Text("Vsync on, the %u fps cap applies when it's off", settings.FrameCap);
		else
			ImGui::Text("Vsync off, capped at %u fps", settings.FrameCap);
		ImGui::Separator();
		const GraphicsTier tier = app.GetRecommendedGraphics();
		const GraphicsPreset& preset = GraphicsPresets[size_t(tier)];
		ImGui::Text("Recommended for this GPU: the %s tier", GraphicsTierNames[size_t(tier)]);
		ImGui::TextDisabled("Indirect light %s, shadows %s, cap %u fps", GiQualityNames[size_t(preset.Gi)], ShadowQualityNames[size_t(preset.Shadows)], preset.FrameCap);
		const bool recommended = settings.Gi == preset.Gi && settings.Shadows == preset.Shadows && settings.FrameCap == preset.FrameCap;
		ImGui::BeginDisabled(recommended);
		if (ImGui::Button(recommended ? "Using the recommended settings" : "Use the recommended settings"))
			app.ApplyRecommendedGraphics();
		ImGui::EndDisabled();
		ImGui::TextDisabled("Integrated GPUs and cards under 3 GB get low,");
		ImGui::TextDisabled("cards under 5.5 GB medium, bigger cards high.");
	}

	void PerformanceWindow::DrawSystem(GameApp& app)
	{
		const Rdn::DeviceProperties& gpu = app.GetDeviceProperties();
		ImGui::TextUnformatted(gpu.Name.c_str());
		ImGui::TextDisabled("%s, %.1f GB of device memory", gpu.Discrete ? "Discrete GPU" : gpu.Integrated ? "Integrated GPU sharing system memory" : "Not a hardware GPU", Gigabytes(gpu.DeviceLocalMemory));
		ImGui::Text("Driver %s, Vulkan %u.%u.%u", gpu.Driver.c_str(), (gpu.ApiVersion >> 22) & 0x7Fu, (gpu.ApiVersion >> 12) & 0x3FFu, gpu.ApiVersion & 0xFFFu);
		ImGui::Text("Video memory %.2f GB used of a %.2f GB budget", Gigabytes(m_Memory.Used), Gigabytes(m_Memory.Budget));
		ImGui::TextDisabled("%.2f GB of it allocated by the game", Gigabytes(m_Memory.Allocated));
		const Rdn::Extent2D extent = app.GetExtent();
		ImGui::Text("%u x %u %s, UI scale %.2f", extent.Width, extent.Height, app.GetSettings().Fullscreen ? "fullscreen" : "windowed", double(app.GetUi().GetScale()));
		ImGui::Text("Presenting with %s", PresentModeName(app.GetPresentMode()));
	}
}
