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

enum class OpType
{
	EdgeRotate,
	VertexRotate,
	DiagonalCollapse,
	EdgeCollapse
};
static OpType op = OpType::EdgeRotate;

static auto IntersectRayLineSegment(const glm::vec3 &rayOrg, const glm::vec3 &rayDir, const glm::vec3 &p0, const glm::vec3 &p1, float &outDist, float &outDepth) -> bool;
static auto IntersectRayTriangle(const glm::vec3 &rayOrg, const glm::vec3 &rayDir, const glm::vec3 &p0, const glm::vec3 &p1, const glm::vec3 &p2, float &outDist) -> bool;
static auto PointToVec3(const OpenMesh::Vec3f &v) -> glm::vec3;
static OpenMesh::HalfedgeHandle GetHalfedgeFromFaceAndVertex(const PolyMesh &mesh, OpenMesh::FaceHandle fh, OpenMesh::VertexHandle vh);

template <typename SpecificHandle>
SpecificHandle HandleCast(OpenMesh::BaseHandle h) { return SpecificHandle(h.idx()); }

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

	m_material = Material{
		glm::vec3{0.4f},
		glm::vec3{0.8f},
		glm::vec3{0.2f},
		2.0f,
		0.0f};
	m_lights = std::vector<Light>{
		Light{
			s_lightPos,
			glm::vec3{2.0f},
			glm::vec3{1.0f},
			1.0, 0.14f, 0.07f}};

	LoadModels();

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
auto App::OnUpdate() -> void
{
	auto M = glm::translate(glm::mat4{1.0f}, glm::vec3{0.0f}) * glm::mat4_cast(m_quat) * glm::scale(glm::mat4{1.0f}, glm::vec3{m_modelScale});
	auto Minv = glm::inverse(M);
	auto V = m_camera->GetView();
	auto P = m_camera->GetProjection();
	auto MVP = P * V * M;

	m_renderState =
		{
			.MVP = MVP,
			.Model = M,
			.ModelInverse = Minv,
			.View = V,
			.Projection = P,
			.Material = &m_material,
			.Lights = &m_lights,
			.Texture = nullptr,
			.CameraPosition = m_camera->GetPosition()};
}
auto App::OnRender() -> void
{
	m_framebuffer->Bind();

	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (m_renderMesh)
		m_renderMesh->Draw(m_renderState);

	if (m_isHovering)
	{
		glDisable(GL_DEPTH_TEST);
		m_hover.second->Draw(m_renderState);
		glEnable(GL_DEPTH_TEST);
	}

	m_framebuffer->Unbind();

	OnImGuiRender();
}
auto App::OnMousePressed(uint32_t button, uint32_t x, uint32_t y) -> void
{
	if (!m_canRotate)
		return;

	if (button == GLFW_MOUSE_BUTTON_LEFT && m_isHovering)
	{
		if (op == OpType::EdgeRotate)
			EdgeRotate(m_topologyMesh, HandleCast<OpenMesh::EdgeHandle>(m_hover.first));
		else if (op == OpType::VertexRotate)
			VertexRotate(m_topologyMesh, HandleCast<OpenMesh::VertexHandle>(m_hover.first));
		else if (op == OpType::DiagonalCollapse)
			DiagonalCollapse(m_topologyMesh, HandleCast<OpenMesh::HalfedgeHandle>(m_hover.first));
		else if (op == OpType::EdgeCollapse)
			EdgeCollapse(m_topologyMesh, m_topologyMesh.halfedge_handle(
											 HandleCast<OpenMesh::EdgeHandle>(m_hover.first), 0));

		FinalizeOperation(m_topologyMesh);

		m_renderMesh->UpdateGPU();
		m_isHovering = false;
	}
	else if (button == GLFW_MOUSE_BUTTON_RIGHT)
	{
		m_prevArcball = ScreenToArcball(x, y);
		m_rotating = true;
	}
}
auto App::OnMouseReleased(uint32_t button, uint32_t x, uint32_t y) -> void
{
	if (!m_canRotate)
		return;

	if (button == GLFW_MOUSE_BUTTON_RIGHT)
		m_rotating = false;
}
auto App::OnMouseMotion(int px, int py) -> void
{
	// hover logic
	if (m_renderMesh)
	{
		static constexpr float eps = 0.01f;

		glm::vec3 rayOrg, rayDir;
		GetLocalRay(px, py, rayOrg, rayDir);

		if (op == OpType::EdgeRotate || op == OpType::EdgeCollapse)
		{
			static OpenMesh::EdgeHandle lastBestEdge;

			OpenMesh::EdgeHandle bestEdge;
			float minDepth = std::numeric_limits<float>::max();

			float dist, depth;
			for (auto eh : m_topologyMesh.edges())
				if (IntersectRayEdge(rayOrg, rayDir, eh, dist, depth))
					if (dist < eps / m_modelScale && depth < minDepth)
						minDepth = depth, bestEdge = eh;

			if (bestEdge.is_valid())
			{
				if (bestEdge != lastBestEdge || !m_isHovering)
				{
					m_hover.second->Vtx().clear();
					HoverEdge(m_topologyMesh, bestEdge);
					lastBestEdge = bestEdge;
					m_isHovering = true;
				}
			}
			else
			{
				m_isHovering = false;
				lastBestEdge = OpenMesh::EdgeHandle{-1};
			}
		}
		else if (op == OpType::VertexRotate)
		{
			static OpenMesh::VertexHandle lastBestVertex;

			OpenMesh::VertexHandle bestVertex;
			float minDepth = std::numeric_limits<float>::max();
			float dist, depth;
			for (auto vh : m_topologyMesh.vertices())
				if (IntersectRayVertex(rayOrg, rayDir, vh, dist, depth))
					if (dist < eps / m_modelScale && depth < minDepth)
						minDepth = depth, bestVertex = vh;

			if (bestVertex.is_valid())
			{
				if (bestVertex != lastBestVertex || !m_isHovering)
				{
					m_hover.second->Vtx().clear();
					HoverVertex(m_topologyMesh, bestVertex);
					lastBestVertex = bestVertex;
					m_isHovering = true;
				}
			}
			else
			{
				m_isHovering = false;
				lastBestVertex = OpenMesh::VertexHandle{-1};
			}
		}
		else if (op == OpType::DiagonalCollapse)
		{
			static OpenMesh::HalfedgeHandle lastBestDiagonalHE;

			OpenMesh::FaceHandle bestFace;
			float minDepth = std::numeric_limits<float>::max();
			float depth;
			for (auto fh : m_topologyMesh.faces())
				if (IntersectRayFace(rayOrg, rayDir, fh, depth))
					if (depth < minDepth)
						minDepth = depth, bestFace = fh;

			if (bestFace.is_valid())
			{
				auto &mesh = m_topologyMesh;
				const auto &vBegin = mesh.fv_begin(bestFace);

				auto vIter = vBegin;
				auto d00 = PointToVec3(mesh.point(*vIter));
				auto d10 = PointToVec3(mesh.point(*(++vIter)));
				auto d01 = PointToVec3(mesh.point(*(++vIter)));
				auto d11 = PointToVec3(mesh.point(*(++vIter)));

				float d0, d1, _0, _1;
				IntersectRayLineSegment(rayOrg, rayDir, d00, d01, d0, _0);
				IntersectRayLineSegment(rayOrg, rayDir, d10, d11, d1, _1);

				const auto &vd0 = vBegin;
				auto vd1 = vd0;
				++vd1;
				auto vd = (d0 < d1) ? vd0 : vd1;
				auto bestDiagonalHE = GetHalfedgeFromFaceAndVertex(mesh, bestFace, *vd);

				if (bestDiagonalHE != lastBestDiagonalHE || !m_isHovering)
				{
					m_hover.second->Vtx().clear();
					HoverDiagonal(m_topologyMesh, bestDiagonalHE);
					lastBestDiagonalHE = bestDiagonalHE;
					m_isHovering = true;
				}
			}
			else
			{
				m_isHovering = false;
				lastBestDiagonalHE = OpenMesh::HalfedgeHandle{-1};
			}
		}
	}

	// rotation logic
	if (m_canRotate && m_rotating)
	{
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
			m_topologyMesh = *model;
			m_renderMesh = std::make_unique<MeshRenderer>(m_topologyMesh, *m_faceShader, *m_edgeShader, *m_pointShader, s_initialColor, s_edgeColor, s_initialColor);
			m_renderMesh->SetDrawMode(true, true, false);
			m_simplifier = std::make_unique<MeshSimplifier>(m_topologyMesh);

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
	if (drawModeChanged && m_renderMesh)
		m_renderMesh->SetDrawMode(m_showFaces, m_showEdges, m_showPoints);

	ImGui::SliderFloat("Scale", &m_modelScale, 0.1f, 4.0f);

	static std::vector<const char *> items{"Edge Rotate", "Vertex Rotate", "Diagonal Collapse", "Edge Collapse"};
	ImGui::Combo("Operation", (int *)&op, items.data(), (int)items.size());

	if (ImGui::Button("Reset"))
		Reset();

	static int simplifySteps = 1;
	ImGui::InputInt("Steps", &simplifySteps);
	if (ImGui::Button("Simplify"))
	{
		m_simplifier->Simplify(simplifySteps);
		FinalizeOperation(m_topologyMesh);

		m_renderMesh->UpdateGPU();
	}

	if (!m_stats.empty())
	{
		ImGui::Separator();
		for (const auto &stat : m_stats)
			ImGui::Text("%s", stat.c_str());
	}

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
		mesh->request_face_status();
		mesh->request_edge_status();
		mesh->request_vertex_status();
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
	m_stats.clear();
	m_hover = std::make_pair<OpenMesh::BaseHandle, std::unique_ptr<Renderable3D>>(OpenMesh::BaseHandle{-1}, std::make_unique<Renderable3D>(*m_edgeShader, GL_LINES, s_selectionColor));

	if (m_renderMesh)
	{
		m_renderMesh = std::make_unique<MeshRenderer>(m_topologyMesh, *m_faceShader, *m_edgeShader, *m_pointShader, s_initialColor, s_edgeColor, s_initialColor);
		m_renderMesh->SetDrawMode(true, true, false);
		m_simplifier = std::make_unique<MeshSimplifier>(m_topologyMesh);

		m_stats.push_back(std::format("vtx: {}", m_topologyMesh.n_vertices()));
		m_stats.push_back(std::format("faces: {}", m_topologyMesh.n_faces()));
		m_stats.push_back(std::format("mu: {}", GetMu(m_topologyMesh)));
		m_stats.push_back(std::format("var: {}", GetLengthVariance(m_topologyMesh)));
	}
}

auto App::GetMu(PolyMesh &mesh) -> float
{
	float totalArea = 0.0f;
	size_t faceCount = 0;

	for (auto f_it = mesh.faces_begin(); f_it != mesh.faces_end(); ++f_it)
	{
		auto fv_it = mesh.fv_iter(*f_it);
		auto a = mesh.point(*fv_it);
		auto b = mesh.point(*(++fv_it));
		auto c = mesh.point(*(++fv_it));
		auto d = mesh.point(*(++fv_it));

		totalArea += 0.5f * ((b - a).cross(c - a)).length();
		totalArea += 0.5f * ((d - c).cross(a - c)).length();

		faceCount++;
	}

	auto mu = std::sqrtf(totalArea / static_cast<float>(faceCount));

	return mu;
}
auto App::GetLengthVariance(PolyMesh &mesh) -> float
{
	float mu = GetMu(mesh);
	float dmu = std::sqrtf(2.0f) * mu;
	float totalDiff = 0.0f, diff;

	for (auto f_it = mesh.faces_begin(); f_it != mesh.faces_end(); ++f_it)
	{
		auto fv_it = mesh.fv_iter(*f_it);
		auto a = mesh.point(*fv_it);
		auto b = mesh.point(*(++fv_it));
		auto c = mesh.point(*(++fv_it));
		auto d = mesh.point(*(++fv_it));

		diff = (b - a).length() - mu;
		totalDiff += diff * diff;
		diff = (c - b).length() - mu;
		totalDiff += diff * diff;
		diff = (d - c).length() - mu;
		totalDiff += diff * diff;
		diff = (a - d).length() - mu;
		totalDiff += diff * diff;

		diff = (c - a).length() - dmu;
		totalDiff += diff * diff;
		diff = (d - b).length() - dmu;
		totalDiff += diff * diff;
	}

	return totalDiff;
}

auto App::EdgeRotate(PolyMesh &mesh, OpenMesh::EdgeHandle eh) const -> void
{
	if (mesh.is_boundary(eh))
		return;

	auto he0 = mesh.halfedge_handle(eh, 0);
	auto he1 = mesh.halfedge_handle(eh, 1);

	auto f0 = mesh.face_handle(he0);
	auto f1 = mesh.face_handle(he1);

	// 0 -- 3 is the given edge
	//
	// 1 -- 0 -- 5
	// |    |    |
	// 2 -- 3 -- 4
	auto v0 = mesh.to_vertex_handle(he0);
	auto v3 = mesh.to_vertex_handle(he1);

	auto v1 = mesh.to_vertex_handle(mesh.next_halfedge_handle(he0));
	auto v2 = mesh.to_vertex_handle(mesh.next_halfedge_handle(mesh.next_halfedge_handle(he0)));

	auto v4 = mesh.to_vertex_handle(mesh.next_halfedge_handle(he1));
	auto v5 = mesh.to_vertex_handle(mesh.next_halfedge_handle(mesh.next_halfedge_handle(he1)));

	mesh.delete_face(f0, false);
	mesh.delete_face(f1, false);

	std::vector<PolyMesh::VertexHandle> nf1 = {v1, v2, v3, v4};
	std::vector<PolyMesh::VertexHandle> nf2 = {v4, v5, v0, v1};

	mesh.add_face(nf1);
	mesh.add_face(nf2);
}
auto App::VertexRotate(PolyMesh &mesh, OpenMesh::VertexHandle vh) const -> void
{
	if (mesh.is_boundary(vh))
		return;

	std::vector<PolyMesh::VertexHandle> neighbors;
	for (auto voh_it = mesh.voh_iter(vh); voh_it.is_valid(); ++voh_it)
	{
		neighbors.push_back(mesh.to_vertex_handle(mesh.next_halfedge_handle(*voh_it)));
		neighbors.push_back(mesh.to_vertex_handle(*voh_it));
	}

	std::vector<PolyMesh::FaceHandle> oldFaces;
	for (auto fh : mesh.vf_range(vh))
		oldFaces.push_back(fh);
	for (auto fh : oldFaces)
		mesh.delete_face(fh, false);

	auto n = neighbors.size();
	for (size_t i = 0; i < n; i += 2)
	{
		std::vector<PolyMesh::VertexHandle> nf =
			{
				vh,
				neighbors[(i + 2) % n],
				neighbors[(i + 1) % n],
				neighbors[i],
			};
		mesh.add_face(nf);
	}
}
auto App::DiagonalCollapse(PolyMesh &mesh, OpenMesh::HalfedgeHandle heh) const -> void
{
	const auto fh = mesh.face_handle(heh);
	const auto vh0 = mesh.from_vertex_handle(heh);
	const auto vh1 = mesh.to_vertex_handle(mesh.next_halfedge_handle(heh));

	auto vhs = std::vector<OpenMesh::VertexHandle>{};
	auto addNewFacesData = [&](OpenMesh::VertexHandle vh)
	{
		for (auto voh_it = mesh.voh_iter(vh); voh_it.is_valid(); ++voh_it)
		{
			if (mesh.face_handle(*voh_it) == fh)
				continue;

			auto voh = *voh_it;
			for (size_t i = 1; i <= 3; i++)
			{
				vhs.push_back(mesh.to_vertex_handle(voh));
				voh = mesh.next_halfedge_handle(voh);
			}
		}
	};

	auto newV = (mesh.point(vh0) + mesh.point(vh1)) * 0.5f;
	addNewFacesData(vh0);
	addNewFacesData(vh1);

	mesh.delete_vertex(vh0, false);
	mesh.delete_vertex(vh1, false);

	auto newVh = mesh.add_vertex(newV);
	for (auto chunk : vhs | std::views::chunk(3))
		mesh.add_face(newVh, chunk[0], chunk[1], chunk[2]);

	std::vector<OpenMesh::VertexHandle> neighbors;
	for (auto v : mesh.vv_range(newVh))
		neighbors.push_back(v);

	for (auto n : neighbors)
		if (mesh.valence(n) == 1)
			RemoveSinglet(mesh, n);
}
auto App::EdgeCollapse(PolyMesh &mesh, OpenMesh::HalfedgeHandle heh) const -> void
{
	auto findFaceFromDiagonal = [&](OpenMesh::VertexHandle v0, OpenMesh::VertexHandle v1) -> OpenMesh::FaceHandle
	{
		for (auto fh : mesh.vf_range(v0))
			for (auto vh : mesh.fv_range(fh))
				if (vh == v1)
					return fh;

		return OpenMesh::FaceHandle{-1};
	};

	auto vh0 = mesh.from_vertex_handle(heh);
	auto vh1 = mesh.to_vertex_handle(heh);

	VertexRotate(mesh, vh0);
	DiagonalCollapse(mesh, GetHalfedgeFromFaceAndVertex(mesh, findFaceFromDiagonal(vh0, vh1), vh0));
}

auto App::RemoveSinglet(PolyMesh &mesh, OpenMesh::VertexHandle vh) const -> void
{
	auto heh0 = *mesh.voh_iter(vh);
	auto heh1 = mesh.next_halfedge_handle(heh0);

	auto fh1 = mesh.face_handle(mesh.opposite_halfedge_handle(heh1));

	// if there's no neighbor, just kill the dangling singlet
	if (!fh1.is_valid())
	{
		mesh.delete_vertex(vh, true);
		return;
	}

	std::vector<OpenMesh::VertexHandle> neighborVhs;
	for (auto v : mesh.fv_range(fh1))
		neighborVhs.push_back(v);

	mesh.delete_vertex(vh, true);
	mesh.delete_edge(mesh.edge_handle(heh1), true);
	mesh.garbage_collection();

	mesh.add_face(neighborVhs);

	std::cout << "singlet removed" << std::endl;
}

auto App::FinalizeOperation(PolyMesh &mesh) const -> void
{
	mesh.garbage_collection();
	mesh.update_normals();
	mesh.update_face_normals();
}

auto App::HoverEdge(PolyMesh &mesh, OpenMesh::EdgeHandle eh) -> void
{
	m_hover.first = eh;

	const auto &renderable = m_hover.second;

	auto heh = mesh.halfedge_handle(eh, 0);
	auto p0 = mesh.point(mesh.from_vertex_handle(heh));
	auto p1 = mesh.point(mesh.to_vertex_handle(heh));

	renderable->Vtx().emplace_back(p0[0], p0[1], p0[2]);
	renderable->Vtx().emplace_back(p1[0], p1[1], p1[2]);
	renderable->UpdateGPU();
};
auto App::HoverVertex(PolyMesh &mesh, OpenMesh::VertexHandle vh) -> void
{
	for (auto ve_iter = mesh.ve_begin(vh); ve_iter != mesh.ve_end(vh); ++ve_iter)
		HoverEdge(mesh, *ve_iter);

	m_hover.first = vh;
}
auto App::HoverDiagonal(PolyMesh &mesh, OpenMesh::HalfedgeHandle heh) -> void
{
	m_hover.first = heh;

	const auto &renderable = m_hover.second;

	auto p0 = mesh.point(mesh.from_vertex_handle(heh));
	auto p1 = mesh.point(mesh.to_vertex_handle(mesh.next_halfedge_handle(heh)));

	renderable->Vtx().emplace_back(p0[0], p0[1], p0[2]);
	renderable->Vtx().emplace_back(p1[0], p1[1], p1[2]);
	renderable->UpdateGPU();
}

auto App::GetLocalRay(int x, int y, glm::vec3 &outOrigin, glm::vec3 &outDir) const -> void
{
	auto ndc2 = ScreenToNDC(x, y);
	auto ndc = glm::vec4{ndc2.x, ndc2.y, -1.0f, 1.0f};
	auto VPinv = glm::inverse(m_renderState.Projection * m_renderState.View);

	auto nearPt = VPinv * glm::vec4{ndc.x, ndc.y, -1.0f, 1.0f};
	nearPt /= nearPt.w;

	auto farPt = VPinv * glm::vec4{ndc.x, ndc.y, 1.0f, 1.0f};
	farPt /= farPt.w;

	auto worldOrigin = glm::vec3{nearPt};
	auto worldDir = glm::normalize(glm::vec3{farPt - nearPt});

	auto &Minv = m_renderState.ModelInverse;
	outOrigin = glm::vec3(Minv * glm::vec4(worldOrigin, 1.0f));
	outDir = glm::normalize(glm::vec3(Minv * glm::vec4(worldDir, 0.0f)));
}
auto App::IntersectRayEdge(const glm::vec3 &rayOrg, const glm::vec3 &rayDir, OpenMesh::EdgeHandle eh, float &outDist, float &outDepth) const -> bool
{
	auto &mesh = m_topologyMesh;
	auto heh = m_topologyMesh.halfedge_handle(eh, 0);
	auto A = PointToVec3(mesh.point(mesh.from_vertex_handle(heh)));
	auto B = PointToVec3(mesh.point(mesh.to_vertex_handle(heh)));

	return IntersectRayLineSegment(rayOrg, rayDir, A, B, outDist, outDepth);
}
auto App::IntersectRayVertex(const glm::vec3 &rayOrg, const glm::vec3 &rayDir, OpenMesh::VertexHandle vh, float &outDist, float &outDepth) const -> bool
{
	auto &mesh = m_topologyMesh;
	glm::vec3 P = PointToVec3(mesh.point(vh));

	glm::vec3 oc = P - rayOrg;

	float t = glm::dot(oc, rayDir);
	if (t < 0)
		return false;

	glm::vec3 pointOnRay = rayOrg + t * rayDir;

	outDist = glm::distance(pointOnRay, P);
	outDepth = t;

	return true;
}
auto App::IntersectRayFace(const glm::vec3 &rayOrg, const glm::vec3 &rayDir, OpenMesh::FaceHandle fh, float &outDist) const -> bool
{
	const auto &mesh = m_topologyMesh;

	auto fv_it = mesh.cfv_begin(fh);
	auto A = PointToVec3(mesh.point(*fv_it));
	auto B = PointToVec3(mesh.point(*(++fv_it)));
	auto C = PointToVec3(mesh.point(*(++fv_it)));
	auto D = PointToVec3(mesh.point(*(++fv_it)));

	float dist;
	if (IntersectRayTriangle(rayOrg, rayDir, A, B, C, dist))
	{
		outDist = dist;
		return true;
	}
	if (IntersectRayTriangle(rayOrg, rayDir, A, C, D, dist))
	{
		outDist = dist;
		return true;
	}
	return false;
}

auto App::ScreenToNDC(int x, int y) const -> glm::vec2
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
auto App::ScreenToArcball(int x, int y) const -> glm::vec3
{
	auto ndc = glm::vec3{ScreenToNDC(x, y), 0.0f};
	float len2 = ndc.x * ndc.x + ndc.y * ndc.y;

	if (len2 <= 1.0f) // on the sphere
		ndc.z = sqrt(1.0f - len2);
	else // outside the sphere, project to edge
		ndc = glm::normalize(ndc);

	return ndc;
}

static auto IntersectRayLineSegment(const glm::vec3 &rayOrg, const glm::vec3 &rayDir, const glm::vec3 &p0, const glm::vec3 &p1, float &outDist, float &outDepth) -> bool
{
	auto u = rayDir;
	auto v = p1 - p0;
	auto w0 = rayOrg - p0;

	float a = glm::dot(u, u);
	float b = glm::dot(u, v);
	float c = glm::dot(v, v);
	float d = glm::dot(u, w0);
	float e = glm::dot(v, w0);

	float denominator = a * c - b * b;
	float s, t;

	// if denominator is 0, lines are parallel
	if (denominator < 1e-6f)
	{
		s = 0.0f;
		t = (b > c ? d / b : e / c);
	}
	else
	{
		s = (b * e - c * d) / denominator;
		t = (a * e - b * d) / denominator;
	}

	// clamp t to the segment [0, 1], recompute s
	t = glm::clamp(t, 0.0f, 1.0f);
	s = (b * t - d) / a;

	glm::vec3 pClosest = rayOrg + s * u;
	glm::vec3 qClosest = p0 + t * v;

	outDist = glm::length(pClosest - qClosest);
	outDepth = s;

	return s > 0;
}
static auto IntersectRayTriangle(const glm::vec3 &rayOrg, const glm::vec3 &rayDir, const glm::vec3 &p0, const glm::vec3 &p1, const glm::vec3 &p2, float &outDist) -> bool
{
	auto e1 = p1 - p0;
	auto e2 = p2 - p0;
	auto n = glm::cross(e1, e2);

	auto rayOrgToP0 = p0 - rayOrg;
	float denom = glm::dot(n, rayDir);
	if (std::abs(denom) < 1e-6f)
		return false; // ray is parallel to triangle

	float t = glm::dot(n, rayOrgToP0) / denom;
	if (t < 0.0f) // behind camera
		return false;

	auto p = rayOrg + t * rayDir;

	auto c0 = glm::cross(p1 - p0, p - p0);
	auto c1 = glm::cross(p2 - p1, p - p1);
	auto c2 = glm::cross(p0 - p2, p - p2);

	if (glm::dot(n, c0) >= 0.0f && glm::dot(n, c1) >= 0.0f && glm::dot(n, c2) >= 0.0f)
	{
		outDist = t;
		return true;
	}
	return false;
}

static auto PointToVec3(const OpenMesh::Vec3f &v) -> glm::vec3
{
	return glm::vec3{v[0], v[1], v[2]};
}
static OpenMesh::HalfedgeHandle GetHalfedgeFromFaceAndVertex(const PolyMesh &mesh, OpenMesh::FaceHandle fh, OpenMesh::VertexHandle vh)
{
	for (auto heh : mesh.fh_range(fh))
		if (mesh.from_vertex_handle(heh) == vh)
			return heh;

	return OpenMesh::HalfedgeHandle{-1};
}