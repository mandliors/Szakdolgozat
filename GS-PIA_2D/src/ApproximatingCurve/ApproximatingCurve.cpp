#include "ApproximatingCurve.hpp"

int32_t ApproximatingCurve::s_curveResolution = 7;

ApproximatingCurve::ApproximatingCurve(Shader &shader, const glm::vec4 &color)
{
    m_cps = std::make_unique<Renderable2D>(shader, GL_POINTS, color);
    m_crv = std::make_unique<Renderable2D>(shader, GL_LINE_LOOP, color);
}

auto ApproximatingCurve::UpdateGPU() -> void
{
    m_cps->UpdateGPU();
    m_crv->UpdateGPU();
}
auto ApproximatingCurve::Draw() const -> void
{
    glDisable(GL_DEPTH_TEST);
    glPointSize(20.0f);
    m_cps->Draw();
    glEnable(GL_DEPTH_TEST);

    m_crv->Draw();
}
auto ApproximatingCurve::ResetCurve() -> void
{
    m_crv->Vtx() = Subdivide(m_cps->Vtx());
}

auto ApproximatingCurve::SetClosed(bool closed) -> void
{
    m_closed = closed;
    m_crv->SetType(closed ? GL_LINE_LOOP : GL_LINE_STRIP);
}
auto ApproximatingCurve::Cps() -> std::vector<glm::vec2> &
{
    return m_cps->Vtx();
}

auto ApproximatingCurve::Subdivide(const std::vector<glm::vec2> &curvePts) -> std::vector<glm::vec2>
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
