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
#include <iostream>

namespace fs = std::filesystem;

App::App(uint32_t width, uint32_t height, std::string_view title)
	: BaseApp(width, height, title)
{
}

auto App::OnInit() -> void
{
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	glPointSize(s_pointSize);
	glLineWidth(s_lineWidth);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	m_faceShader = std::make_unique<FaceShader>();
	m_edgeShader = std::make_unique<EdgeShader>();
	m_pointShader = std::make_unique<PointShader>();

	m_camera = std::make_unique<Camera>(
		glm::vec3{0.0f, 1.5f, 4.0f},
		glm::vec3{0.0f, 0.0f, 0.0f},
		glm::vec3{0.0f, 1.0f, 0.0f},
		45.0f,
		static_cast<float>(m_width) / static_cast<float>(m_height),
		1.0f,
		20.0f);

	LoadModels();

	m_limitPt = std::make_unique<Renderable3D>(*m_pointShader, GL_POINTS, s_limitPtColor);
	m_solver = nullptr;

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

	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	auto M = glm::translate(glm::mat4{1.0f}, glm::vec3{0.0f}) * glm::mat4_cast(m_quat) * glm::scale(glm::mat4{1.0f}, glm::vec3{m_modelScale});
	auto Minv = glm::inverse(M);
	auto V = m_camera->GetView();
	auto P = m_camera->GetProjection();
	auto MVP = P * V * M;

	auto material = Material{
		glm::vec3{0.4f},
		glm::vec3{0.8f},
		glm::vec3{0.2f},
		2.0f,
		0.0f};
	auto lights = std::vector<Light>{
		Light{
			s_lightPos,
			glm::vec3{2.0f},
			glm::vec3{1.0f},
			1.0, 0.14f, 0.07f}};

	RenderState renderState{
		.MVP = MVP,
		.Model = M,
		.ModelInverse = Minv,
		.View = V,
		.Projection = P,
		.Material = &material,
		.Lights = &lights,
		.Texture = nullptr,
		.CameraPosition = m_camera->GetPosition()};

	if (m_originalMesh)
		m_originalMesh->Draw(renderState);
	if (m_iteratedMesh)
		m_iteratedMesh->Draw(renderState);

	glDisable(GL_DEPTH_TEST);
	m_limitPt->UpdateGPU();
	m_limitPt->Draw(renderState);
	glEnable(GL_DEPTH_TEST);

	m_framebuffer->Unbind();

	OnImGuiRender();
}
auto App::OnMousePressed(uint32_t button, uint32_t x, uint32_t y) -> void
{
	if (!m_canRotate)
		return;

	m_prevArcball = ScreenToArcball(x, y);
	m_rotating = true;
}
auto App::OnMouseReleased(uint32_t button, uint32_t x, uint32_t y) -> void
{
	if (!m_canRotate)
		return;

	m_rotating = false;
}
auto App::OnMouseMotion(int px, int py) -> void
{
	if (!m_canRotate || !m_rotating)
		return;

	glm::vec3 axis, currArcball = ScreenToArcball(px, py);
	float angle = 0.0f;

	axis = glm::cross(m_prevArcball, currArcball);
	angle = acos(glm::clamp(glm::dot(m_prevArcball, currArcball), -1.0f, 1.0f));

	float axisLength = length(axis);
	if (axisLength > std::numeric_limits<float>::epsilon())
	{
		axis = glm::normalize(axis);
		glm::quat delta = glm::angleAxis(angle, axis);
		m_quat = glm::normalize(delta * m_quat);
	}
	m_prevArcball = currArcball;
}
auto App::OnDestroy() -> void
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
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

	if (const auto &io = ImGui::GetIO(); io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
	{
		ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
	}

	ImGui::End();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
	ImGui::Begin("Viewport");
	{
		bool windowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

		static auto s_viewportSize = glm::vec2{0.0f, 0.0f};
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		if ((s_viewportSize != *((glm::vec2 *)&viewportPanelSize) && viewportPanelSize.x > 0 && viewportPanelSize.y > 0))
		{
			s_viewportSize = {viewportPanelSize.x, viewportPanelSize.y};
			m_framebuffer->Resize((uint32_t)s_viewportSize.x, (uint32_t)s_viewportSize.y);
		}
		m_camera->Resize(viewportPanelSize.x / viewportPanelSize.y);
		ImGui::Image(reinterpret_cast<void *>(m_framebuffer->GetColorAttachmentRendererID()), ImVec2{s_viewportSize.x, s_viewportSize.y}, ImVec2{0, 1}, ImVec2{1, 0});

		bool imageHovered = ImGui::IsItemHovered();
		m_canRotate = (windowHovered && imageHovered);
	}
	ImGui::End();
	ImGui::PopStyleVar();

	ImGui::Begin("Models");

	static char pathBuf[64] = "assets/models";
	ImGui::InputText("##path", pathBuf, sizeof(pathBuf));
	ImGui::SameLine();
	if (ImGui::Button("Load"))
	{
		m_modelsPath = std::string{pathBuf};
		LoadModels();
	}

	int i = 0;
	for (const auto &[name, model] : m_models)
	{
		bool selected = (m_selectedModelIndex == i);

		ImGui::RadioButton(name.c_str(), selected);
		if (ImGui::IsItemClicked())
		{
			m_selectedModelIndex = i;

			if (!m_originalMesh)
			{
				m_originalMesh = std::make_unique<ApproximatingMesh>(*model, *m_faceShader, *m_edgeShader, *m_pointShader, s_initialColor, s_edgeColor, s_initialColor);
				m_iteratedMesh = std::make_unique<ApproximatingMesh>(*model, *m_faceShader, *m_edgeShader, *m_pointShader, s_iteratedColor, s_edgeColor, s_iteratedColor);
				m_solver = std::make_unique<Solver3D>(m_originalMesh->TopologyMesh(), m_iteratedMesh->TopologyMesh());
			}
			else
			{
				m_originalMesh->TopologyMesh() = *model;
				m_iteratedMesh->TopologyMesh() = *model;
			}

			Reset();
		}

		i++;
	}

	ImGui::End();

	ImGui::Begin("Settings");

	bool drawModeChanged = false;
	drawModeChanged |= ImGui::Checkbox("Vertices", &m_showPoints);
	ImGui::SameLine();
	drawModeChanged |= ImGui::Checkbox("Edges", &m_showEdges);
	ImGui::SameLine();
	drawModeChanged |= ImGui::Checkbox("Faces", &m_showFaces);
	if (drawModeChanged && m_originalMesh && m_iteratedMesh)
	{
		m_originalMesh->RenderMesh().SetDrawMode(m_showFaces, m_showEdges, m_showPoints);
		m_iteratedMesh->RenderMesh().SetDrawMode(m_showFaces, m_showEdges, m_showPoints);
	}

	ImGui::SliderFloat("Scale", &m_modelScale, 0.1f, 2.0f);
	if (ImGui::SliderInt("Resolution", &m_surfaceResolution, 1, 8))
		ApproximatingMesh::SetSurfaceResolution(m_surfaceResolution), Reset();

	if (ImGui::Button("Step") && m_solver)
	{
		m_solver->StepVertex();
		if (auto limitPt = m_solver->GetLimitPoint())
			m_limitPt->Vtx()[0] = *limitPt;
		else
			m_limitPt->Vtx().clear();
			
		m_iteratedMesh->ResetSurface();
		m_iteratedMesh->UpdateGPU();
	}
	ImGui::SameLine();
	if (ImGui::Button("Iterate") && m_solver)
	{
		m_solver->Iterate();
		if (auto limitPt = m_solver->GetLimitPoint())
			m_limitPt->Vtx()[0] = *limitPt;
		else
			m_limitPt->Vtx().clear();

		m_iteratedMesh->ResetSurface();
		m_iteratedMesh->UpdateGPU();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset"))
		Reset();

	ImGui::End();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

auto App::LoadModel(std::string_view path) -> std::unique_ptr<PolyMesh>
{
	auto mesh = std::make_unique<PolyMesh>();

	OpenMesh::IO::Options opt;
	opt += OpenMesh::IO::Options::VertexNormal;
	opt += OpenMesh::IO::Options::FaceNormal;

	if (!OpenMesh::IO::read_mesh(*mesh, std::string{path}, opt))
		std::cerr << "failed to load model\n";
	else // if the file didn't contain normals (we need per-vertex normals)
	{
		if (!mesh->has_vertex_normals())
		{
			mesh->request_vertex_normals();
			mesh->update_normals();
		}
		if (!mesh->has_face_normals())
		{
			mesh->request_face_normals();
			mesh->update_face_normals();
		}
	}

	return mesh;
}
auto App::LoadModels() -> void
{
	m_models.clear();
	if (!fs::exists(m_modelsPath))
		return;

	for (const auto &entry : fs::directory_iterator(m_modelsPath))
		if (entry.is_regular_file() && entry.path().extension() == ".obj")
			m_models.emplace(
				entry.path().filename().string(),
				LoadModel(entry.path().string()));
}

auto App::Reset() -> void
{
	m_steps = 0;
	m_iterations = 0;
	m_quat = glm::quat{1.0f, 0.0f, 0.0f, 0.0f};

	if (m_originalMesh)
		m_originalMesh->ResetSurface(), m_originalMesh->UpdateGPU();

	if (m_iteratedMesh)
	{
		m_iteratedMesh->TopologyMesh() = m_originalMesh->TopologyMesh();
		m_iteratedMesh->ResetSurface(), m_iteratedMesh->UpdateGPU();
	}

	if (m_solver)
	{
		m_solver->Reset();
		if (auto limitPt = m_solver->GetLimitPoint())
			m_limitPt->Vtx() = {*limitPt};
		else
			m_limitPt->Vtx().clear();
	}
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
auto App::ScreenToArcball(int x, int y) -> glm::vec3
{
	auto ndc = glm::vec3{ScreenToNDC(x, y), 0.0f};
	float len2 = ndc.x * ndc.x + ndc.y * ndc.y;

	if (len2 <= 1.0f) // on the sphere
		ndc.z = sqrt(1.0f - len2);
	else // outside the sphere, project to edge
		ndc = glm::normalize(ndc);

	return ndc;
}
