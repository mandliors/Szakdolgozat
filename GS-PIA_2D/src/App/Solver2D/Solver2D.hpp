#include "glm/glm.hpp"

#include <vector>
#include <optional>

class Solver2D
{
public:
    Solver2D(std::vector<glm::vec2> &initialCps, std::vector<glm::vec2> &iteratedCps);

    auto Reset() -> void;
    auto GetAlpha() const -> float;
    auto GetLimitPoint() const -> std::optional<glm::vec2>;
    auto StepVertex() -> void;
    auto Iterate() -> void;

    auto SetClosed(bool closed) -> void { m_closed = closed; }
    auto GetIterations() const -> size_t { return m_iterations; }
    auto GetSteps() const -> size_t { return m_steps; }

private:
    std::vector<glm::vec2> &m_originalCps;
    std::vector<glm::vec2> &m_iteratedCps;

    size_t m_steps = 0;
    size_t m_iterations = 0;

    bool m_closed = true;
};