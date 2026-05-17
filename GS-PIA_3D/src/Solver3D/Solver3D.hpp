#include "glm/glm.hpp"
#include "OpenMesh/Core/IO/MeshIO.hh"
#include "OpenMesh/Core/Mesh/PolyMesh_ArrayKernelT.hh"

#include <vector>
#include <optional>

typedef OpenMesh::PolyMesh_ArrayKernelT<> PolyMesh;

class Solver3D
{
public:
    Solver3D(PolyMesh &originalMesh, PolyMesh &iteratedMesh)
        : m_originalMesh(originalMesh), m_iteratedMesh(iteratedMesh)
    {
    }

    auto Reset() -> void;
    auto GetAlpha() const -> float;
    auto GetLimitPoint() const -> std::optional<glm::vec3>;
    auto StepVertex() -> void;
    auto Iterate() -> void;

    auto SetClosed(bool closed) -> void { m_closed = closed; }
    auto GetIterations() const -> size_t { return m_iterations; }
    auto GetSteps() const -> size_t { return m_steps; }

private:
    PolyMesh &m_originalMesh;
    PolyMesh &m_iteratedMesh;

    size_t m_steps = 0;
    size_t m_iterations = 0;

    bool m_closed = true;
};