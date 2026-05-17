#pragma once

#include "glm/glm.hpp"

#include "BaseApp/BaseApp.hpp"
#include "Camera/Camera.hpp"
#include "Shader/Shader.hpp"
#include "Framebuffer/Framebuffer.hpp"
#include "Renderable2D.hpp"
#include "ApproximatingCurve/ApproximatingCurve.hpp"
#include "Solver2D/Solver2D.hpp"

#include <vector>
#include <memory>
#include <string>

namespace fs = std::filesystem;

class App : public BaseApp
{
public:
	App(uint32_t width, uint32_t height, std::string_view title);

	auto OnInit() -> void override;
	auto OnRender() -> void override;
	auto OnDestroy() -> void override;

	auto OnMousePressed(uint32_t button, uint32_t x, uint32_t y) -> void override;
	auto OnMouseReleased(uint32_t button, uint32_t x, uint32_t y) -> void override;
	auto OnMouseMotion(int x, int y) -> void override;

private:
	auto OnImGuiInit() const -> void;
	auto OnImGuiRender() -> void;

	auto Reset() -> void;

	auto ScreenToNDC(int x, int y) -> glm::vec2;

private:
	static constexpr auto s_curveResolution = 7;

	static inline const auto s_initialColor = glm::vec4{0.55f, 0.3f, 1.0f, 0.8f};
	static inline const auto s_iteratedColor = glm::vec4{0.1f, 0.8f, 0.7f, 0.8f};
	static inline const auto s_limitPtColor = glm::vec4{0.9f, 0.9f, 0.2f, 0.8f};

private:
	std::unique_ptr<Shader> m_shader;

	std::unique_ptr<ApproximatingCurve> m_originalCurve;
	std::unique_ptr<ApproximatingCurve> m_iteratedCurve;
	std::unique_ptr<Renderable2D> m_limitPt;

	std::unique_ptr<Solver2D> m_solver;

	bool m_closed = true;
	glm::vec2 *m_draggedVertex = nullptr;

	std::unique_ptr<Framebuffer> m_framebuffer;
};