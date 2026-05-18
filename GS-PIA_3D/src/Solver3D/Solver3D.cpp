#include "Solver3D.hpp"

auto Solver3D::Reset() -> void
{
    m_steps = 0;
    m_iterations = 0;
}
auto Solver3D::GetAlpha() const -> float
{
    const auto &mesh = m_iteratedMesh;
    auto vh = mesh.vertex_handle(m_steps);
    auto n = mesh.valence(vh);

    if (mesh.is_boundary(vh))
        return 4.0f / 6.0f;
    else
        return n * n / static_cast<float>(n * (n + 5));
}

auto Solver3D::GetLimitPoint() const -> std::optional<glm::vec3>
{
    const auto &mesh = m_iteratedMesh;
    auto vh = mesh.vertex_handle(m_steps);
    PolyMesh::Point limitPt;

    // open mesh, we are on a boundary, apply [1 4 1]/6 stencil
    if (mesh.is_boundary(vh))
    {
        auto bh = mesh.halfedge_handle(vh);
        auto nextVh = mesh.to_vertex_handle(bh);
        auto prevVh = mesh.to_vertex_handle(mesh.opposite_halfedge_handle(mesh.prev_halfedge_handle(bh)));

        limitPt = (1.0f * mesh.point(prevVh) + 4.0f * mesh.point(vh) + 1.0f * mesh.point(nextVh)) / 6.0f;
    }
    else
    {
        auto n = mesh.valence(vh);
        auto alpha = GetAlpha();

        auto faceMult = 1.0f / (n * (n + 5));
        PolyMesh::Point facePtSum{0.0f, 0.0f, 0.0f};
        std::vector<PolyMesh::Point> faceCentroids;
        for (auto fh : mesh.vf_range(vh))
        {
            facePtSum += mesh.calc_face_centroid(fh);
            faceCentroids.push_back(mesh.calc_face_centroid(fh));
        }

        auto edgeMult = 4.0f / (n * (n + 5));
        PolyMesh::Point edgePt0Sum{0.0f, 0.0f, 0.0f};
        PolyMesh::Point edgePt1Sum{0.0f, 0.0f, 0.0f};
        uint32_t j = 0;
        for (auto ohh : mesh.voh_range(vh))
        {
            auto e0 = mesh.point(mesh.to_vertex_handle(ohh));
            auto &fPrev = faceCentroids[(j - 1 + n) % n];
            auto &fCurr = faceCentroids[j];
            auto &v0 = mesh.point(vh);
            auto e1 = (v0 + e0 + fPrev + fCurr) / 4.0f;

            edgePt0Sum += e0;
            edgePt1Sum += e1;
            j++;
        }

        const auto &v0 = m_iteratedMesh.point(vh);
        auto v0Mult = (n - 2) / static_cast<float>(n);
        auto mult = 1.0f / (n * n);
        auto v1 = v0Mult * v0 + mult * edgePt0Sum + mult * facePtSum;

        limitPt = alpha * v1 + edgeMult * edgePt1Sum + faceMult * facePtSum;
    }

    return {{limitPt[0], limitPt[1], limitPt[2]}};
}
auto Solver3D::StepVertex() -> void
{
    const auto alpha = GetAlpha();

    auto &originalMesh = m_originalMesh;
    auto &iteratedMesh = m_iteratedMesh;

    auto ovh = originalMesh.vertex_handle(m_steps);
    auto ivh = iteratedMesh.vertex_handle(m_steps);

    const auto &v0 = originalMesh.point(ovh);
    auto &v = iteratedMesh.point(ivh);
    const auto limitPt = GetLimitPoint().value_or(glm::vec3{0.0f});
    const auto &l = PolyMesh::Point{limitPt.x, limitPt.y, limitPt.z};

    v += (1.0f / alpha) * (v0 - l);

    m_steps = (m_steps + 1) % originalMesh.n_vertices();
    if (m_steps == 0)
        m_iterations++;
}
auto Solver3D::Iterate() -> void
{
    for (size_t i = m_steps; i < m_originalMesh.n_vertices(); i++)
        StepVertex();
}
