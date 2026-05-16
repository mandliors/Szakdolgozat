#pragma once

#include "Renderable2D.hpp"

#include <memory>

class ApproximatingCurve
{
public:
    ApproximatingCurve(Shader &shader, const glm::vec4 &color);

    auto UpdateGPU() -> void;
    auto Draw() const -> void;
    auto ResetCurve() -> void;

    auto SetClosed(bool closed) -> void;
    auto Cps() -> std::vector<glm::vec2> &;

private:
    auto Subdivide(const std::vector<glm::vec2> &curvePts) -> std::vector<glm::vec2>;

public:
    static auto SetCurveResolution(int32_t resolution) -> void;

private:
    static int32_t s_curveResolution;

private:
    std::unique_ptr<Renderable2D> m_cps;
    std::unique_ptr<Renderable2D> m_crv;

    bool m_closed = true;
};