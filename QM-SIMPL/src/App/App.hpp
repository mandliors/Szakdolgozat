#pragma once

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "OpenMesh/Core/IO/MeshIO.hh"
#include "OpenMesh/Core/Mesh/PolyMesh_ArrayKernelT.hh"

#include "BaseApp/BaseApp.hpp"
#include "Camera/Camera.hpp"
#include "MeshRenderer/Shaders/FaceShader.hpp"
#include "MeshRenderer/Shaders/EdgeShader.hpp"
#include "MeshRenderer/Shaders/PointShader.hpp"
#include "MeshRenderer/MeshRenderer.hpp"
#include "Framebuffer/Framebuffer.hpp"
#include "../MeshSimplifier/MeshSimplifier.hpp"
#include "../Renderable3D.hpp"

#include <vector>
#include <memory>
#include <string>

namespace fs = std::filesystem;

using PolyMesh = OpenMesh::PolyMesh_ArrayKernelT<>;

class App : public BaseApp
{
public:
	App(uint32_t width, uint32_t height, std::string_view title);

	auto OnInit() -> void override;
	auto OnRender() -> void override;
	auto OnUpdate() -> void override;
	auto OnMousePressed(uint32_t button, uint32_t x, uint32_t y) -> void override;
	auto OnMouseReleased(uint32_t button, uint32_t x, uint32_t y) -> void override;
	auto OnMouseMotion(int px, int py) -> void override;
	auto OnDestroy() -> void override;

private:
	auto OnImGuiInit() const -> void;
	auto OnImGuiRender() -> void;

	auto LoadModel(std::string_view path) -> std::unique_ptr<PolyMesh>;
	auto LoadModels() -> void;

	auto Reset() -> void;

	// paper's algorithm implementations
	auto GetMu(PolyMesh &mesh) -> float;
	auto GetLengthVariance(PolyMesh &mesh) -> float;

	auto EdgeRotate(PolyMesh &mesh, OpenMesh::EdgeHandle eh) const -> void;
	auto VertexRotate(PolyMesh &mesh, OpenMesh::VertexHandle vh) const -> void;
	auto DiagonalCollapse(PolyMesh &mesh, OpenMesh::HalfedgeHandle heh) const -> void;
	auto EdgeCollapse(PolyMesh &mesh, OpenMesh::HalfedgeHandle heh) const -> void;

	auto RemoveSinglet(PolyMesh &mesh, OpenMesh::VertexHandle vh) const -> void;

	auto FinalizeOperation(PolyMesh &mesh) const -> void;

	auto HoverEdge(PolyMesh &mesh, OpenMesh::EdgeHandle eh) -> void;
	auto HoverVertex(PolyMesh &mesh, OpenMesh::VertexHandle vh) -> void;
	auto HoverDiagonal(PolyMesh &mesh, OpenMesh::HalfedgeHandle heh) -> void;

	auto GetLocalRay(int x, int y, glm::vec3 &outOrigin, glm::vec3 &outDir) const -> void;
	auto IntersectRayEdge(const glm::vec3 &rayOrg, const glm::vec3 &rayDir, OpenMesh::EdgeHandle eh, float &outDist, float &outDepth) const -> bool;
	auto IntersectRayVertex(const glm::vec3 &rayOrg, const glm::vec3 &rayDir, OpenMesh::VertexHandle vh, float &outDist, float &outDepth) const -> bool;
	auto IntersectRayFace(const glm::vec3 &rayOrg, const glm::vec3 &rayDir, OpenMesh::FaceHandle fh, float &outDist) const -> bool;

	auto ScreenToNDC(int x, int y) const -> glm::vec2;
	auto ScreenToArcball(int x, int y) const -> glm::vec3;

private:
	static constexpr auto s_pointSize = 10;
	static constexpr auto s_lineWidth = 5;

	static inline const auto s_initialColor = glm::vec4{0.55f, 0.3f, 1.0f, 1.0f};
	static inline const auto s_iteratedColor = glm::vec4{0.1f, 0.8f, 0.7f, 1.0f};
	static inline const auto s_edgeColor = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f};
	static inline const auto s_selectionColor = glm::vec4{0.96f, 0.67f, 0.26f, 1.0f};
	static inline const auto s_lightPos = glm::vec4{2.0f, 1.0f, 2.0f, 0.0f};

private:
	std::unique_ptr<FaceShader> m_faceShader;
	std::unique_ptr<EdgeShader> m_edgeShader;
	std::unique_ptr<PointShader> m_pointShader;
	std::unique_ptr<Camera> m_camera;

	PolyMesh m_topologyMesh;
	std::unique_ptr<MeshRenderer> m_renderMesh;
	std::unique_ptr<MeshSimplifier> m_simplifier;

	Material m_material;
	std::vector<Light> m_lights;
	RenderState m_renderState;

	std::string m_modelsPath{"assets/models"};
	std::map<std::string, std::unique_ptr<PolyMesh>, std::less<>> m_models;
	int m_selectedModelIndex = -1;

	std::pair<OpenMesh::BaseHandle, std::unique_ptr<Renderable3D>> m_hover;
	bool m_isHovering = false;

	bool m_showFaces = true;
	bool m_showEdges = true;
	bool m_showPoints = false;
	float m_modelScale = 1.0f;

	std::vector<std::string> m_stats;

	bool m_canRotate = false;
	bool m_rotating = false;
	glm::vec3 m_prevArcball{0.0f, 0.0f, 1.0f};
	glm::quat m_quat{1.0f, 0.0f, 0.0f, 0.0f};

	uint32_t m_steps = 0;
	uint32_t m_iterations = 0;

	std::unique_ptr<Framebuffer> m_framebuffer;
};