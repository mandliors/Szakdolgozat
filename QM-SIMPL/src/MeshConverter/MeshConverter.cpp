#include "MeshConverter.hpp"
#include <iostream>
#include <ranges>

MeshConverter::MeshConverter(PolyMesh &mesh) : m_mesh(mesh)
{
    m_mesh.add_property(m_squareness);
    m_mesh.add_property(m_selected);
    m_mesh.add_property(m_flagged);

    for (auto eh : m_mesh.edges())
    {
        m_mesh.property(m_squareness, eh) = 0;
        m_mesh.property(m_selected, eh) = false;
        m_mesh.property(m_flagged, eh) = false;
    }

    SetEdgeSquarenessValues();
    SelectEdges();
    FlagEdges();
}

auto MeshConverter::TopologyMesh() -> PolyMesh & { return m_mesh; }

auto MeshConverter::Execute() -> void
{
    DissolveEdges();

    // MergeRemainingTriangles();
}

auto MeshConverter::DissolveEdges() -> void
{
    for (auto eh : m_mesh.edges())
        if (m_mesh.property(m_flagged, eh))
            DissolveEdge(eh);
}
auto MeshConverter::MergeRemainingTriangles() -> void
{
    bool triangleExists = true;
    while (triangleExists)
    {
        triangleExists = false;
        for (auto fh : m_mesh.faces())
        {
            if (m_mesh.valence(fh) == 3)
            {
                SolveTriangle(fh);
                triangleExists = true;
                break;
            }
        }
    }
}

auto MeshConverter::DissolveEdge(OpenMesh::EdgeHandle eh) -> OpenMesh::FaceHandle
{
    auto heh0 = m_mesh.halfedge_handle(eh, 0);
    auto heh1 = m_mesh.halfedge_handle(eh, 1);

    std::vector<OpenMesh::VertexHandle> vertices;

    for (auto heh = heh0; m_mesh.next_halfedge_handle(heh) != heh0; heh = m_mesh.next_halfedge_handle(heh))
    {
        if (!heh.is_valid())
            break;

        vertices.push_back(m_mesh.to_vertex_handle(heh));
    }
    for (auto heh = heh1; m_mesh.next_halfedge_handle(heh) != heh1; heh = m_mesh.next_halfedge_handle(heh))
    {
        if (!heh.is_valid())
            break;

        vertices.push_back(m_mesh.to_vertex_handle(heh));
    }

    m_mesh.delete_edge(eh, false);
    auto nfh = m_mesh.add_face(vertices);

    return nfh;
}
auto MeshConverter::SetEdgeSquarenessValues() -> void
{
    for (auto eh : m_mesh.edges())
        m_mesh.property(m_squareness, eh) = CalculateEdgeSquareness(eh);
}
auto MeshConverter::SelectEdges() -> void
{
    for (auto fh : m_mesh.faces())
    {
        auto heh0 = m_mesh.halfedge_handle(fh);
        auto heh1 = m_mesh.next_halfedge_handle(heh0);
        auto heh2 = m_mesh.next_halfedge_handle(heh1);

        auto eh0 = m_mesh.edge_handle(heh0);
        auto eh1 = m_mesh.edge_handle(heh1);
        auto eh2 = m_mesh.edge_handle(heh2);

        auto s0 = m_mesh.property(m_squareness, eh0);
        auto s1 = m_mesh.property(m_squareness, eh1);
        auto s2 = m_mesh.property(m_squareness, eh2);

        if (s0 < 0 && s1 < 0 && s2 < 0)
            continue;

        if (s0 >= s1 && s0 >= s2)
            m_mesh.property(m_selected, eh0) = true;
        else if (s1 >= s0 && s1 >= s2)
            m_mesh.property(m_selected, eh1) = true;
        else
            m_mesh.property(m_selected, eh2) = true;
    }
}
auto MeshConverter::FlagEdges() -> void
{
    for (auto eh : m_mesh.edges())
    {
        if (m_mesh.property(m_selected, eh))
        {
            auto hehA = m_mesh.halfedge_handle(eh, 0);
            auto hehB = m_mesh.halfedge_handle(eh, 1);

            auto heh0 = m_mesh.next_halfedge_handle(hehA);
            auto heh1 = m_mesh.next_halfedge_handle(heh0);
            auto heh2 = m_mesh.next_halfedge_handle(hehB);
            auto heh3 = m_mesh.next_halfedge_handle(heh2);

            auto eh0 = m_mesh.edge_handle(heh0);
            auto eh1 = m_mesh.edge_handle(heh1);
            auto eh2 = m_mesh.edge_handle(heh2);
            auto eh3 = m_mesh.edge_handle(heh3);

            if (m_mesh.property(m_selected, eh0) && m_mesh.property(m_squareness, eh0) > m_mesh.property(m_squareness, eh))
                continue;
            if (m_mesh.property(m_selected, eh1) && m_mesh.property(m_squareness, eh1) > m_mesh.property(m_squareness, eh))
                continue;
            if (m_mesh.property(m_selected, eh2) && m_mesh.property(m_squareness, eh2) > m_mesh.property(m_squareness, eh))
                continue;
            if (m_mesh.property(m_selected, eh3) && m_mesh.property(m_squareness, eh3) > m_mesh.property(m_squareness, eh))
                continue;

            m_mesh.property(m_flagged, eh) = true;

            m_mesh.property(m_selected, eh0) = false;
            m_mesh.property(m_selected, eh1) = false;
            m_mesh.property(m_selected, eh2) = false;
            m_mesh.property(m_selected, eh3) = false;
        }
    }
}
auto MeshConverter::CalculateEdgeSquareness(OpenMesh::EdgeHandle eh) -> float
{
    if (!eh.is_valid() || m_mesh.is_boundary(eh))
        return -1.0f;

    auto heh0 = m_mesh.halfedge_handle(eh, 0);
    auto heh1 = m_mesh.next_halfedge_handle(heh0);
    auto heh2 = m_mesh.halfedge_handle(eh, 1);
    auto heh3 = m_mesh.next_halfedge_handle(heh2);

    auto fh0 = m_mesh.face_handle(heh0);
    auto fh1 = m_mesh.face_handle(heh2);
    if (!fh0.is_valid() || !fh1.is_valid())
        return -1.0f;

    auto p0 = m_mesh.point(m_mesh.to_vertex_handle(heh0));
    auto p1 = m_mesh.point(m_mesh.to_vertex_handle(heh1));
    auto p2 = m_mesh.point(m_mesh.to_vertex_handle(heh2));
    auto p3 = m_mesh.point(m_mesh.to_vertex_handle(heh3));

    return CalculateSquarenessImpl({p0, p1, p2, p3});
}
auto MeshConverter::CalculateSquarenessImpl(const std::array<OpenMesh::Vec3f, 4> &vtx) -> float
{
    auto e01 = (vtx[1] - vtx[0]).normalized();
    auto e12 = (vtx[2] - vtx[1]).normalized();
    auto e23 = (vtx[3] - vtx[2]).normalized();
    auto e30 = (vtx[0] - vtx[3]).normalized();

    float dot0 = std::abs((-e01) | e12);
    float dot1 = std::abs((-e12) | e23);
    float dot2 = std::abs((-e23) | e30);
    float dot3 = std::abs((-e30) | e01);

    return 1.0f - 0.25f * (dot0 + dot1 + dot2 + dot3);
}

auto MeshConverter::SolveTriangle(OpenMesh::FaceHandle fh) -> void
{
    auto path = FindPathToNearestTriangle(fh);
    if (path.empty())
        return;

    auto findMutualEdge = [&](OpenMesh::FaceHandle f1, OpenMesh::FaceHandle f2) -> OpenMesh::EdgeHandle
    {
        for (auto eh : m_mesh.fe_range(f1))
        {
            if (m_mesh.is_boundary(eh))
                continue;

            auto heh0 = m_mesh.halfedge_handle(eh, 0);
            auto heh1 = m_mesh.halfedge_handle(eh, 1);

            auto fh0 = m_mesh.face_handle(heh0);
            auto fh1 = m_mesh.face_handle(heh1);

            if ((fh0 == f1 && fh1 == f2) || (fh0 == f2 && fh1 == f1))
                return eh;
        }

        return OpenMesh::EdgeHandle{-1};
    };

    // neighbors
    if (path.size() == 2)
    {
        auto eh = findMutualEdge(path[0], path[1]);
        assert(eh.is_valid() && "no mutual edge??");

        DissolveEdge(eh);

        return;
    }

    auto cfh = path.back();
    for (auto [nfh, nnfh] : path | std::views::reverse | std::views::drop(1) | std::views::adjacent<2>)
    {
        auto meh = findMutualEdge(cfh, nfh);
        assert(meh.is_valid() && "no mutual edge #2??");

        auto pfh = DissolveEdge(meh);
        assert(pfh.is_valid() && "pentagon created");

        auto pmeh = findMutualEdge(pfh, nnfh);
        assert(pmeh.is_valid() && "no mutual edge #3??");

        auto heh = m_mesh.halfedge_handle(pmeh, 0);
        if (m_mesh.face_handle(heh) != pfh)
            heh = m_mesh.halfedge_handle(pmeh, 1);
        assert(m_mesh.face_handle(heh) == pfh && "pmeh doesn't border pentagon?");

        auto vhs = std::vector<OpenMesh::VertexHandle>(5, OpenMesh::VertexHandle{-1});
        for (size_t i = 0; i < 5; i++)
        {
            vhs[i] = m_mesh.to_vertex_handle(heh);
            heh = m_mesh.next_halfedge_handle(heh);
        }

        auto sq1 = CalculateSquarenessImpl({m_mesh.point(vhs[0]),
                                            m_mesh.point(vhs[1]),
                                            m_mesh.point(vhs[2]),
                                            m_mesh.point(vhs[3])});
        auto sq2 = CalculateSquarenessImpl({m_mesh.point(vhs[1]),
                                            m_mesh.point(vhs[2]),
                                            m_mesh.point(vhs[3]),
                                            m_mesh.point(vhs[4])});

        auto primaryVh = (sq1 < sq2) ? vhs[0] : vhs[4];
        auto fallbackVh = (sq1 < sq2) ? vhs[4] : vhs[0];

        cfh = CutTriangle(pfh, primaryVh);
        if (!cfh.is_valid())
        {
            cfh = CutTriangle(pfh, fallbackVh);
            assert(cfh.is_valid() && "both ears infeasible for pentagon cut");
        }

        if (m_mesh.valence(nnfh) == 3)
        {
            auto eh = findMutualEdge(cfh, nnfh);
            assert(eh.is_valid() && "no mutual edge #4??");
            DissolveEdge(eh);

            break;
        }
    }
}

auto MeshConverter::FindPathToNearestTriangle(OpenMesh::FaceHandle fh)
    -> std::vector<OpenMesh::FaceHandle>
{
    if (!fh.is_valid())
        return {};

    std::queue<OpenMesh::FaceHandle> queue;

    OpenMesh::FPropHandleT<bool> visited;
    m_mesh.add_property(visited);

    std::unordered_map<int, OpenMesh::FaceHandle> parent;

    queue.push(fh);
    m_mesh.property(visited, fh) = true;

    OpenMesh::FaceHandle tfh = OpenMesh::FaceHandle{-1};

    while (!queue.empty())
    {
        auto cfh = queue.front();
        queue.pop();

        for (auto nfh : m_mesh.ff_range(cfh))
        {
            if (!nfh.is_valid())
                continue;

            if (!m_mesh.property(visited, nfh))
            {
                m_mesh.property(visited, nfh) = true;
                parent[nfh.idx()] = cfh;

                if (m_mesh.valence(nfh) == 3)
                {
                    tfh = nfh;
                    break;
                }

                queue.push(nfh);
            }
        }
        if (tfh.is_valid())
            break;
    }

    std::vector<OpenMesh::FaceHandle> path;

    if (tfh.is_valid())
    {
        for (auto curr = tfh; curr.is_valid();)
        {
            path.push_back(curr);
            if (curr == fh)
                break;

            auto it = parent.find(curr.idx());
            curr = (it != parent.end()) ? it->second : OpenMesh::FaceHandle{-1};
        }
        std::reverse(path.begin(), path.end());
    }

    m_mesh.remove_property(visited);

    return path;
}
auto MeshConverter::CutTriangle(OpenMesh::FaceHandle fh, OpenMesh::VertexHandle vh) -> OpenMesh::FaceHandle
{
    auto vhs = std::vector<OpenMesh::VertexHandle>{};
    for (auto vh : m_mesh.fv_range(fh))
        vhs.push_back(vh);

    auto vhit = std::find(vhs.begin(), vhs.end(), vh);
    assert(vhit != vhs.end() && "vertex not found in triangle");
    auto vhi = static_cast<size_t>(std::distance(vhs.begin(), vhit));

    auto nvhs = std::vector<OpenMesh::VertexHandle>{};
    for (size_t i = 0; i < vhs.size(); i++)
        if (i != vhi)
            nvhs.push_back(vhs[i]);

    auto tvh0 = vhs[(vhi + vhs.size() - 1) % vhs.size()];
    auto tvh1 = vhs[vhi];
    auto tvh2 = vhs[(vhi + 1) % vhs.size()];

    if (m_mesh.find_halfedge(tvh0, tvh2).is_valid())
        return OpenMesh::FaceHandle{-1};

    m_mesh.delete_face(fh, false);

    auto qfh = m_mesh.add_face(nvhs);
    assert(qfh.is_valid() && "failed to create quad");
    auto tfh = m_mesh.add_face({tvh0, tvh1, tvh2});
    assert(tfh.is_valid() && "failed to create triangle");

    return tfh;
}