#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "glm/gtc/matrix_transform.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

#include "App.hpp"
#include "Rendering/RenderState.hpp"

#include <array>
#include <ranges>

namespace fs = std::filesystem;

App::App(uint32_t width, uint32_t height, std::string_view title)
	: BaseApp(width, height, title)
{
}

auto App::OnInit() -> void
{
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glLineWidth(5.0f);

	m_shader = std::make_unique<Shader>(
		std::vector<std::pair<GLenum, fs::path>>{
			{GL_VERTEX_SHADER, "assets/shaders/2dbasic.vs"},
			{GL_FRAGMENT_SHADER, "assets/shaders/2dbasic.fs"},
		});

	m_initialCps = std::make_unique<Renderable2D>(*m_shader, GL_POINTS, s_initialColor);
	m_iteratedCps = std::make_unique<Renderable2D>(*m_shader, GL_POINTS, s_iteratedColor);
	m_limitPt = std::make_unique<Renderable2D>(*m_shader, GL_POINTS, s_limitPtColor);

	m_originalCurve = std::make_unique<Renderable2D>(*m_shader, GL_LINE_LOOP, s_initialColor);
	m_iteratedCurve = std::make_unique<Renderable2D>(*m_shader, GL_LINE_LOOP, s_iteratedColor);

	m_initialCps->Vtx() = std::vector<glm::vec2>{
		glm::vec2{-0.45f, -0.3f},
		glm::vec2{-0.15f, 0.35f},
		glm::vec2{0.2f, 0.45f},
		glm::vec2{0.5f, -0.3f},
		glm::vec2{0.0f, -0.45f},
	};
	Reset();

	// ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui_ImplGlfw_InitForOpenGL(m_window, true);
	ImGui_ImplOpenGL3_Init();

	m_framebuffer = std::make_unique<Framebuffer>(m_width, m_height, 1, false);

	OnImGuiInit();
}
auto App::OnRender() -> void
{
	m_framebuffer->Bind();

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	m_shader->Bind(RenderState{});
	m_shader->Use();

	m_originalCurve->UpdateGPU();
	m_originalCurve->Draw();
	m_iteratedCurve->UpdateGPU();
	m_iteratedCurve->Draw();

	glDisable(GL_DEPTH_TEST);
	glPointSize(20.0f);
	m_initialCps->UpdateGPU();
	m_initialCps->Draw();
	glPointSize(15.0f);
	m_iteratedCps->UpdateGPU();
	m_iteratedCps->Draw();
	glPointSize(10.0f);
	m_limitPt->UpdateGPU();
	m_limitPt->Draw();
	glEnable(GL_DEPTH_TEST);

	m_framebuffer->Unbind();

	OnImGuiRender();
}
auto App::OnDestroy() -> void
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

auto App::OnMouseMotion(int x, int y) -> void
{
	if (m_draggedVertex)
	{
		*m_draggedVertex = ScreenToNDC(x, y);
		Reset();
	}
}
auto App::OnMousePressed(uint32_t button, uint32_t x, uint32_t y) -> void
{
	if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
		for (auto &p : m_initialCps->Vtx())
		{
			auto mousePos = ScreenToNDC(x, y);
			if (glm::length(mousePos - p) < 0.05f)
			{
				m_draggedVertex = &p;
				return;
			}
		}
	}
	else if (button == GLFW_MOUSE_BUTTON_RIGHT)
	{
		const auto p = ScreenToNDC(x, y);

		m_initialCps->Vtx().push_back(p);
		m_iteratedCps->Vtx().push_back(p);
		Reset();
	}
}
auto App::OnMouseReleased(uint32_t button, uint32_t x, uint32_t y) -> void
{
	m_draggedVertex = nullptr;
}

auto App::OnImGuiInit() const -> void
{
	float scale;
	glfwGetWindowContentScale(m_window, &scale, NULL);

	ImGuiIO &io = ImGui::GetIO();
	io.Fonts->Clear();

	ImFontConfig cfg;
	cfg.SizePixels = 13.0f * scale;

	io.Fonts->AddFontDefault(&cfg);

	ImGui::GetStyle().ScaleAllSizes(scale);
	io.FontGlobalScale = 1.0f;

	ImGuiStyle &style = ImGui::GetStyle();
	style.ScaleAllSizes(scale);
}
auto App::OnImGuiRender() -> void
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	static bool dockspaceOpen = true;
	static bool opt_fullscreen = true;
	static bool opt_padding = false;
	static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
	if (opt_fullscreen)
	{
		ImGuiViewport *viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	}
	else
		dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;

	if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
		window_flags |= ImGuiWindowFlags_NoBackground;
	if (!opt_padding)
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
	if (!opt_padding)
		ImGui::PopStyleVar();

	if (opt_fullscreen)
		ImGui::PopStyleVar(2);

	// DockSpace
	if (const auto &io = ImGui::GetIO(); io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
	{
		ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
	}

	ImGui::End();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
	ImGui::Begin("Viewport");
	{
		static auto s_viewportSize = glm::vec2{0.0f, 0.0f};
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		if ((s_viewportSize != *((glm::vec2 *)&viewportPanelSize) && viewportPanelSize.x > 0 && viewportPanelSize.y > 0))
		{
			s_viewportSize = {viewportPanelSize.x, viewportPanelSize.y};
			m_framebuffer->Resize((uint32_t)s_viewportSize.x, (uint32_t)s_viewportSize.y);
		}
		ImGui::Image(reinterpret_cast<void *>(m_framebuffer->GetColorAttachmentRendererID()), ImVec2{s_viewportSize.x, s_viewportSize.y}, ImVec2{0, 1}, ImVec2{1, 0});
	}
	ImGui::End();
	ImGui::PopStyleVar();

	ImGui::Begin("Actions");
	{
		if (ImGui::Checkbox("Closed Curve", &m_closed))
		{
			if (m_closed)
			{
				m_originalCurve->SetType(GL_LINE_LOOP);
				m_iteratedCurve->SetType(GL_LINE_LOOP);
			}
			else
			{
				m_originalCurve->SetType(GL_LINE_STRIP);
				m_iteratedCurve->SetType(GL_LINE_STRIP);
			}
			Reset();
		}

		if (ImGui::Button("Step"))
			StepVertex();

		ImGui::SameLine();

		if (ImGui::Button("Iterate"))
			Iterate();

		ImGui::SameLine();

		if (ImGui::Button("Reset"))
		{
			m_initialCps->Vtx().clear();
			m_iteratedCps->Vtx().clear();
			Reset();
		}
	}
	ImGui::End();

	ImGui::Begin("Info");
	{
		ImGui::Text("Iterations: %d", m_iterations);
		ImGui::Text("Steps: %d/%d", m_steps, m_initialCps->Vtx().size());

		ImGui::Separator();

		ImGui::Text("Initial Color");
		ImGui::SameLine();
		ImGui::ColorButton("##color1", (const ImVec4 &)s_initialColor,
						   ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
						   ImVec2(40, 40));

		ImGui::Text("Iterated Color");
		ImGui::SameLine();
		ImGui::ColorButton("##color2", (const ImVec4 &)s_iteratedColor,
						   ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
						   ImVec2(40, 40));

		ImGui::Text("Limit Point Color");
		ImGui::SameLine();
		ImGui::ColorButton("##color3", (const ImVec4 &)s_limitPtColor,
						   ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
						   ImVec2(40, 40));
	}
	ImGui::End();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

auto App::Reset() -> void
{
	m_steps = 0;
	m_iterations = 0;

	m_iteratedCps->Vtx() = m_initialCps->Vtx();
	CalculateLimitPoint();

	m_originalCurve->Vtx() = Subdivide(m_initialCps->Vtx());
	m_iteratedCurve->Vtx() = Subdivide(m_iteratedCps->Vtx());
}
auto App::GetAlpha() const -> float
{
	const auto m = m_steps;
	const auto n = m_initialCps->Vtx().size();

	if (m_closed)
		return 4.0f / 6.0f;
	else
	{
		if (n == 3)
			return (m == 0 || m == 2) ? 1.0f : 6.0f / 16.0f;
		else if (n == 4)
			return (m == 0 || m == 3) ? 1.0f : 12.0f / 27.0f;
		else if (n == 5)
		{
			if (m == 0 || m == 4)
				return 1.0f;
			else if (m == 1 || m == 3)
				return 61.0f / 108.0f;
			else // m = 2
				return 2.0f / 4.0f;
		}
		else
		{
			if (m == 0 || m == n - 1)
				return 1.0f;
			else if (m == 1 || m == n - 2)
				return 183.0f / 324.0f;
			else if (m == 2 || m == n - 3)
				return 7.0f / 12.0f;
			else
				return 4.0f / 6.0f;
		}
	}
}
auto App::CalculateLimitPoint() const -> void
{
	const auto &P = m_iteratedCps->Vtx();
	const auto n = P.size();

	if (n == 0)
	{
		m_limitPt->Vtx().clear();
		return;
	}

	auto prevPrev = (m_steps + n - 2) % n;
	auto prev = (m_steps + n - 1) % n;
	auto curr = m_steps;
	auto next = (m_steps + 1) % n;
	auto nextNext = (m_steps + 2) % n;

	if (m_closed)
		m_limitPt->Vtx() = {(1.0f * P[prev] + 4.0f * P[curr] + 1.0f * P[next]) / 6.0f};
	else
	{
		if (n == 3)
		{
			if (curr == 0 || curr == 2)
				m_limitPt->Vtx() = {1.0f * P[curr]};
			else // n = 1
				m_limitPt->Vtx() = {(5.0f * P[prev] + 6.0f * P[curr] + 5.0f * P[next]) / 16.0f};
		}
		else if (n == 4)
		{
			if (curr == 0 || curr == 3)
				m_limitPt->Vtx() = {1.0f * P[curr]};
			else if (curr == 1)
				m_limitPt->Vtx() = {(8.0f * P[prev] + 12.0f * P[curr] + 6.0f * P[next] + 1.0f * P[nextNext]) / 27.0f};
			else // curr = 2
				m_limitPt->Vtx() = {(1.0f * P[prevPrev] + 6.0f * P[prev] + 12.0f * P[curr] + 8.0f * P[next]) / 27.0f};
		}
		else if (n == 5)
		{
			if (curr == 0 || curr == 4)
				m_limitPt->Vtx() = {1.0f * P[curr]};
			else if (curr == 1)
				m_limitPt->Vtx() = {(32.0f * P[prev] + 61.0f * P[curr] + 14.0f * P[next] + 1.0f * P[nextNext]) / 108.0f};
			else if (curr == 3)
				m_limitPt->Vtx() = {(1.0f * P[prevPrev] + 14.0f * P[prev] + 61.0f * P[curr] + 32.0f * P[next]) / 108.0f};
			else // curr = 2
				m_limitPt->Vtx() = {(1.0f * P[prev] + 2.0f * P[curr] + 1.0f * P[next]) / 4.0f};
		}
		else
		{
			if (2 < curr && curr < n - 3)
				m_limitPt->Vtx() = {(1.0f * P[prev] + 4.0f * P[curr] + 1.0f * P[next]) / 6.0f};
			else if (curr == 0 || curr == n - 1)
				m_limitPt->Vtx() = {1.0f * P[curr]};
			else if (curr == 1)
				m_limitPt->Vtx() = {(96.0f * P[prev] + 183.0f * P[curr] + 43.0f * P[next] + 2.0f * P[nextNext]) / 324.0f};
			else if (curr == n - 2)
				m_limitPt->Vtx() = {(2.0f * P[prevPrev] + 43.0f * P[prev] + 183.0f * P[curr] + 96.0f * P[next]) / 324.0f};
			else if (curr == 2)
				m_limitPt->Vtx() = {(3.0f * P[prev] + 7.0f * P[curr] + 2.0f * P[next]) / 12.0f};
			else if (curr == n - 3)
				m_limitPt->Vtx() = {(2.0f * P[prev] + 7.0f * P[curr] + 3.0f * P[next]) / 12.0f};
		}
	}
}

auto App::StepVertex(bool updateCurve) -> void
{
	if (m_initialCps->Vtx().empty())
		return;

	const auto alpha = GetAlpha();
	const auto &v = m_initialCps->Vtx()[m_steps];
	const auto &l = m_limitPt->Vtx()[0];

	m_iteratedCps->Vtx()[m_steps] += (1.0f / alpha) * (v - l);
	if (updateCurve)
		m_iteratedCurve->Vtx() = Subdivide(m_iteratedCps->Vtx());

	m_steps = (m_steps + 1) % m_initialCps->Vtx().size();
	if (m_steps == 0)
		m_iterations++;

	CalculateLimitPoint();
}
auto App::Iterate() -> void
{
	for (size_t i = m_steps; i < m_initialCps->Vtx().size(); i++)
		StepVertex(i + 1 == m_initialCps->Vtx().size());
}
auto App::Subdivide(const std::vector<glm::vec2> &curvePts) -> std::vector<glm::vec2>
{
	if (curvePts.size() < 3)
		return curvePts;

	auto subdivideClosed = [this](const auto &pts)
	{
		auto n = pts.size();
		auto newPts = std::vector<glm::vec2>{};
		newPts.reserve(n * 2);

		for (size_t i = 0; i <= n - 1; i++)
		{
			const auto &posA = pts[(i + n - 1) % n];
			const auto &posB = pts[i];
			const auto &posC = pts[(i + 1) % n];

			const auto newPos1 = (posA + 6.0f * posB + posC) / 8.0f;
			const auto newPos2 = (posB + posC) / 2.0f;

			newPts.push_back(newPos1);
			newPts.push_back(newPos2);
		}

		return newPts;
	};
	auto subdivideOpen = [this](const auto &pts)
	{
		auto n = pts.size();
		auto newPts = std::vector<glm::vec2>{};
		newPts.reserve(n * 2);

		if (n == 3)
		{
			newPts.push_back(pts[0]);
			newPts.push_back(0.5f * pts[0] + 0.5f * pts[1]);
			newPts.push_back(0.5f * pts[1] + 0.5f * pts[2]);
			newPts.push_back(pts[2]);
		}
		else if (n == 4)
		{
			newPts.push_back(pts[0]);
			newPts.push_back(0.5f * pts[0] + 0.5f * pts[1]);
			newPts.push_back(0.5f * pts[1] + 0.5f * pts[2]);
			newPts.push_back(0.5f * pts[2] + 0.5f * pts[3]);
			newPts.push_back(pts[3]);
		}
		else if (n == 5)
		{
			newPts.push_back(pts[0]);
			newPts.push_back(0.5f * pts[0] + 0.5f * pts[1]);
			newPts.push_back(0.75f * pts[1] + 0.25f * pts[2]);
			newPts.push_back(3.0f / 16.0f * pts[1] + 10.0f / 16.0f * pts[2] + 3.0f / 16.0f * pts[3]);
			newPts.push_back(0.25f * pts[2] + 0.75f * pts[3]);
			newPts.push_back(0.5f * pts[3] + 0.5f * pts[4]);
			newPts.push_back(pts[4]);
		}
		else
		{
			newPts.push_back(pts[0]);
			newPts.push_back(0.5f * pts[0] + 0.5f * pts[1]);
			newPts.push_back(0.75f * pts[1] + 0.25f * pts[2]);
			newPts.push_back(3.0f / 16.0f * pts[1] + 11.0f / 16.0f * pts[2] + 2.0f / 16.0f * pts[3]);
			newPts.push_back(0.5f * pts[2] + 0.5f * pts[3]);

			for (size_t i = 3; i < n - 3; i++)
			{
				const auto &posA = pts[i - 1];
				const auto &posB = pts[i];
				const auto &posC = pts[i + 1];

				newPts.push_back(2.0f / 16.0f * posA + 12.0f / 16.0f * posB + 2.0f / 16.0f * posC);
				newPts.push_back((posB + posC) / 2.0f);
			}

			newPts.push_back(2.0f / 16.0f * pts[n - 4] + 11.0f / 16.0f * pts[n - 3] + 3.0f / 16.0f * pts[n - 2]);
			newPts.push_back(0.25f * pts[n - 3] + 0.75f * pts[n - 2]);
			newPts.push_back(0.5f * pts[n - 2] + 0.5f * pts[n - 1]);
			newPts.push_back(pts[n - 1]);
		}

		return newPts;
	};
	auto subdivide = [this, subdivideClosed, subdivideOpen](const auto &pts)
	{
		return m_closed ? subdivideClosed(pts) : subdivideOpen(pts);
	};

	auto newCurvePts = curvePts;
	for (size_t i = 0; i < s_curveResolution; ++i)
		newCurvePts = subdivide(newCurvePts);

	return newCurvePts;
}

auto App::ScreenToNDC(int x, int y) -> glm::vec2
{
	auto win = ImGui::FindWindowByName("Viewport");
	if (!win)
		return glm::vec2{0.0f};

	auto contentMin = ImVec2{
		win->Pos.x + win->WindowPadding.x,
		win->Pos.y + win->TitleBarHeight + win->WindowPadding.y};

	ImVec2 contentSize = win->ContentRegionRect.GetSize();

	auto localX = static_cast<float>(x) - contentMin.x;
	auto localY = static_cast<float>(y) - contentMin.y;

	auto w = contentSize.x;
	auto h = contentSize.y;

	auto ndcX = 2.0f * (localX / w) - 1.0f;
	auto ndcY = -2.0f * (localY / h) + 1.0f;

	return glm::vec2{ndcX, ndcY};
}
