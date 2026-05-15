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
#include "../ApproximatingMesh/ApproximatingMesh.hpp"
#include "Framebuffer/Framebuffer.hpp"
#include "../Renderable3D.hpp"

#include <vector>
#include <memory>
#include <string>

namespace fs = std::filesystem;

typedef OpenMesh::PolyMesh_ArrayKernelT<> PolyMesh;

class App : public BaseApp
{
public:
	App(uint32_t width, uint32_t height, std::string_view title);

	auto OnInit() -> void override;
	auto OnRender() -> void override;
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
	auto GetAlpha() const -> float;
	auto CalculateLimitPoint() const -> void;
	auto StepVertex(bool updateMesh = true) -> void;
	auto Iterate() -> void;

	auto ScreenToNDC(int x, int y) -> glm::vec2;
	auto ScreenToArcball(int x, int y) -> glm::vec3;

private:
	static constexpr auto s_pointSize = 10;
	static constexpr auto s_lineWidth = 5;

	static inline const auto s_initialColor = glm::vec4{0.55f, 0.3f, 1.0f, 1.0f};
	static inline const auto s_iteratedColor = glm::vec4{0.1f, 0.8f, 0.7f, 1.0f};
	static inline const auto s_limitPtColor = glm::vec4{0.9f, 0.9f, 0.2f, 1.0f};
	static inline const auto s_edgeColor = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f};
	static inline const auto s_lightPos = glm::vec4{2.0f, 1.0f, 2.0f, 0.0f};

private:
	std::unique_ptr<FaceShader> m_faceShader;
	std::unique_ptr<EdgeShader> m_edgeShader;
	std::unique_ptr<PointShader> m_pointShader;
	std::unique_ptr<Camera> m_camera;

	std::unique_ptr<ApproximatingMesh> m_originalMesh;
	std::unique_ptr<ApproximatingMesh> m_iteratedMesh;
	std::unique_ptr<Renderable3D> m_limitPt;

	std::string m_modelsPath{"assets/models"};
	std::unordered_map<std::string, std::unique_ptr<PolyMesh>> m_models;
	int m_selectedModelIndex = -1;

	bool m_showFaces = true;
	bool m_showEdges = false;
	bool m_showPoints = false;
	float m_modelScale = 1.0f;
	int32_t m_surfaceResolution = 5;

	bool m_canRotate = false;
	bool m_rotating = false;
	glm::vec3 m_prevArcball{0.0f, 0.0f, 1.0f};
	glm::quat m_quat{1.0f, 0.0f, 0.0f, 0.0f};

	uint32_t m_steps = 0;
	uint32_t m_iterations = 0;

	std::unique_ptr<Framebuffer> m_framebuffer;
};