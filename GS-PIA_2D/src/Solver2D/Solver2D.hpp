#include "glm/glm.hpp"

#include <vector>
#include <optional>

class Solver2D
{
public:
    static auto GetLimitPoint(const std::vector<glm::vec2>& iterCps, int32_t idx, bool closed) -> std::optional<glm::vec2>;
    static auto StepVertex(const std::vector<glm::vec2>& origCps, std::vector<glm::vec2>& iterCps, int32_t idx, bool closed) -> void;
    static auto Iterate(const std::vector<glm::vec2>& origCps, std::vector<glm::vec2>& iterCps, int32_t idx, bool closed) -> void;
    
public:
    Solver2D(const Solver2D& other) = delete;
    auto operator=(const Solver2D& other) -> Solver2D& = delete;

private:
    Solver2D() = default;

private:
    static auto GetAlpha(int32_t idx, int32_t nVertices, bool closed) -> float;
};