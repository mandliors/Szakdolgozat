#include "Solver2D.hpp"

Solver2D::Solver2D(std::vector<glm::vec2> &initialCps, std::vector<glm::vec2> &iteratedCps)
    : m_originalCps(initialCps), m_iteratedCps(iteratedCps)
{
}

auto Solver2D::Reset() -> void
{
    m_steps = 0;
    m_iterations = 0;

    m_iteratedCps = m_originalCps;
}
auto Solver2D::GetAlpha() const -> float
{
    const auto m = m_steps;
    const auto n = m_originalCps.size();

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
auto Solver2D::GetLimitPoint() const -> std::optional<glm::vec2>
{
    const auto &P = m_iteratedCps;
    const auto n = P.size();

    if (n == 0)
        return std::nullopt;

    auto prevPrev = (m_steps + n - 2) % n;
    auto prev = (m_steps + n - 1) % n;
    auto curr = m_steps;
    auto next = (m_steps + 1) % n;
    auto nextNext = (m_steps + 2) % n;

    if (m_closed)
        return std::make_optional<glm::vec2>((1.0f * P[prev] + 4.0f * P[curr] + 1.0f * P[next]) / 6.0f);
    else
    {
        if (n == 3)
        {
            if (curr == 0 || curr == 2)
                return std::make_optional<glm::vec2>({1.0f * P[curr]});
            else // n = 1
                return std::make_optional<glm::vec2>((5.0f * P[prev] + 6.0f * P[curr] + 5.0f * P[next]) / 16.0f);
        }
        else if (n == 4)
        {
            if (curr == 0 || curr == 3)
                return std::make_optional<glm::vec2>({1.0f * P[curr]});
            else if (curr == 1)
                return std::make_optional<glm::vec2>((8.0f * P[prev] + 12.0f * P[curr] + 6.0f * P[next] + 1.0f * P[nextNext]) / 27.0f);
            else // curr = 2
                return std::make_optional<glm::vec2>((1.0f * P[prevPrev] + 6.0f * P[prev] + 12.0f * P[curr] + 8.0f * P[next]) / 27.0f);
        }
        else if (n == 5)
        {
            if (curr == 0 || curr == 4)
                return std::make_optional<glm::vec2>({1.0f * P[curr]});
            else if (curr == 1)
                return std::make_optional<glm::vec2>((32.0f * P[prev] + 61.0f * P[curr] + 14.0f * P[next] + 1.0f * P[nextNext]) / 108.0f);
            else if (curr == 3)
                return std::make_optional<glm::vec2>((1.0f * P[prevPrev] + 14.0f * P[prev] + 61.0f * P[curr] + 32.0f * P[next]) / 108.0f);
            else // curr = 2
                return std::make_optional<glm::vec2>((1.0f * P[prev] + 2.0f * P[curr] + 1.0f * P[next]) / 4.0f);
        }
        else
        {
            if (2 < curr && curr < n - 3)
                return std::make_optional<glm::vec2>((1.0f * P[prev] + 4.0f * P[curr] + 1.0f * P[next]) / 6.0f);
            else if (curr == 0 || curr == n - 1)
                return std::make_optional<glm::vec2>({1.0f * P[curr]});
            else if (curr == 1)
                return std::make_optional<glm::vec2>((96.0f * P[prev] + 183.0f * P[curr] + 43.0f * P[next] + 2.0f * P[nextNext]) / 324.0f);
            else if (curr == n - 2)
                return std::make_optional<glm::vec2>((2.0f * P[prevPrev] + 43.0f * P[prev] + 183.0f * P[curr] + 96.0f * P[next]) / 324.0f);
            else if (curr == 2)
                return std::make_optional<glm::vec2>((3.0f * P[prev] + 7.0f * P[curr] + 2.0f * P[next]) / 12.0f);
            else if (curr == n - 3)
                return std::make_optional<glm::vec2>((2.0f * P[prev] + 7.0f * P[curr] + 3.0f * P[next]) / 12.0f);
        }
    }
}
auto Solver2D::StepVertex() -> void
{
    if (m_originalCps.empty())
        return;

    const auto alpha = GetAlpha();
    const auto &v = m_originalCps[m_steps];
    const auto &l = GetLimitPoint().value_or(v);

    m_iteratedCps[m_steps] += (1.0f / alpha) * (v - l);

    m_steps = (m_steps + 1) % m_originalCps.size();
    if (m_steps == 0)
        m_iterations++;
}
auto Solver2D::Iterate() -> void
{
    for (size_t i = m_steps; i < m_originalCps.size(); i++)
        StepVertex();
}