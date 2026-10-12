#include "MeshConverter.hpp"

#include "DebugRenderer/DebugRenderer.hpp"
#include "Renderables/Renderable3D.hpp"

#include <iostream>
#include <ranges>

static auto FindMutualEdge(const PolyMesh &mesh, OpenMesh::FaceHandle f1, OpenMesh::FaceHandle f2) -> OpenMesh::EdgeHandle;
static auto GatherVerticesAroundEdge(const PolyMesh &mesh, OpenMesh::EdgeHandle eh) -> std::vector<OpenMesh::VertexHandle>;
static auto IsEar(const PolyMesh &mesh, const std::vector<OpenMesh::VertexHandle> &vhs, size_t earIndex) -> bool;

MeshConverter::MeshConverter(PolyMesh &mesh) : m_mesh(mesh)
{
    m_mesh.add_property(m_squareness);
    m_mesh.add_property(m_selected);
    m_mesh.add_property(m_flaggable);
    m_mesh.add_property(m_flagged);

    for (auto eh : m_mesh.edges())
    {
        m_mesh.property(m_squareness, eh) = 0;
        m_mesh.property(m_selected, eh) = false;
        m_mesh.property(m_flaggable, eh) = true;
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
    MergeRemainingTriangles();
}

auto MeshConverter::DissolveEdges() -> void
{
    std::vector<OpenMesh::EdgeHandle> flagged;
    for (auto eh : m_mesh.edges())
        if (m_mesh.property(m_flagged, eh))
            flagged.push_back(eh);

    for (auto eh : flagged)
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
    auto vertices = GatherVerticesAroundEdge(m_mesh, eh);
    m_mesh.delete_edge(eh, false);

    return m_mesh.add_face(vertices);
}
auto MeshConverter::SetEdgeSquarenessValues() -> void
{
    for (auto eh : m_mesh.edges())
        m_mesh.property(m_squareness, eh) = CalculateEdgeSquareness(eh);
}
auto MeshConverter::SelectEdges() -> void
{
    for (auto fh : m_mesh.faces())
        SelectBestEdge(fh);
}
auto MeshConverter::SelectBestEdge(OpenMesh::FaceHandle fh) -> OpenMesh::EdgeHandle
{
    auto heh0 = m_mesh.halfedge_handle(fh);
    auto heh1 = m_mesh.next_halfedge_handle(heh0);
    auto heh2 = m_mesh.next_halfedge_handle(heh1);

    auto eh0 = m_mesh.edge_handle(heh0);
    auto eh1 = m_mesh.edge_handle(heh1);
    auto eh2 = m_mesh.edge_handle(heh2);

    m_mesh.property(m_selected, eh0) = false;
    m_mesh.property(m_selected, eh1) = false;
    m_mesh.property(m_selected, eh2) = false;

    OpenMesh::EdgeHandle best{-1};
    float bestSquareness = -1.0f;

    for (auto eh : {eh0, eh1, eh2})
    {
        if (!m_mesh.property(m_flaggable, eh))
            continue;

        auto sq = m_mesh.property(m_squareness, eh);
        if (sq < 0)
            continue;

        if (!best.is_valid() || sq > bestSquareness)
        {
            best = eh;
            bestSquareness = sq;
        }
    }

    if (best.is_valid())
        m_mesh.property(m_selected, best) = true;

    return best;
}
auto MeshConverter::FlagEdges() -> void
{
    std::vector<OpenMesh::EdgeHandle> flaggableSelected;
    for (auto eh : m_mesh.edges())
        if (m_mesh.property(m_selected, eh) && m_mesh.property(m_flaggable, eh))
            flaggableSelected.push_back(eh);

    while (!flaggableSelected.empty())
    {
        auto newSelections = std::vector<OpenMesh::EdgeHandle>{};
        for (auto eh : flaggableSelected)
        {
            if (!m_mesh.property(m_flaggable, eh) || !m_mesh.property(m_selected, eh))
                continue;

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

            if (m_mesh.property(m_selected, eh0) && m_mesh.property(m_flaggable, eh0) && m_mesh.property(m_squareness, eh0) > m_mesh.property(m_squareness, eh))
                continue;
            if (m_mesh.property(m_selected, eh1) && m_mesh.property(m_flaggable, eh1) && m_mesh.property(m_squareness, eh1) > m_mesh.property(m_squareness, eh))
                continue;
            if (m_mesh.property(m_selected, eh2) && m_mesh.property(m_flaggable, eh2) && m_mesh.property(m_squareness, eh2) > m_mesh.property(m_squareness, eh))
                continue;
            if (m_mesh.property(m_selected, eh3) && m_mesh.property(m_flaggable, eh3) && m_mesh.property(m_squareness, eh3) > m_mesh.property(m_squareness, eh))
                continue;

            m_mesh.property(m_flagged, eh) = true;

            m_mesh.property(m_flaggable, eh) = false;
            m_mesh.property(m_flaggable, eh0) = false;
            m_mesh.property(m_flaggable, eh1) = false;
            m_mesh.property(m_flaggable, eh2) = false;
            m_mesh.property(m_flaggable, eh3) = false;

            auto f0 = m_mesh.face_handle(m_mesh.opposite_halfedge_handle(heh0));
            auto f1 = m_mesh.face_handle(m_mesh.opposite_halfedge_handle(heh1));
            auto f2 = m_mesh.face_handle(m_mesh.opposite_halfedge_handle(heh2));
            auto f3 = m_mesh.face_handle(m_mesh.opposite_halfedge_handle(heh3));

            for (auto f : {f0, f1, f2, f3})
                if (f.is_valid())
                    if (auto eh = SelectBestEdge(f); eh.is_valid())
                        newSelections.push_back(eh);
        }

        flaggableSelected = std::move(newSelections);
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

    auto cfh = path.back();
    for (auto [nfh, nnfh] : path | std::views::reverse | std::views::drop(1) | std::views::adjacent<2>)
    {
        cfh = AdvanceTriangle(cfh, nfh, nnfh);

        if (m_mesh.valence(nnfh) == 3)
        {
            auto eh = FindMutualEdge(m_mesh, cfh, nnfh);
            assert(eh.is_valid() && "no mutual edge");
            DissolveEdge(eh);

            break;
        }
    }
}

auto MeshConverter::AdvanceTriangle(OpenMesh::FaceHandle cfh, OpenMesh::FaceHandle nfh, OpenMesh::FaceHandle nnfh) -> OpenMesh::FaceHandle
{
    // find mutual edges and create a pentagon
    auto meh = FindMutualEdge(m_mesh, cfh, nfh);
    assert(meh.is_valid() && "no mutual edge");

    auto pfh = DissolveEdge(meh);
    assert(pfh.is_valid() && "pentagon created");

    auto pmeh = FindMutualEdge(m_mesh, pfh, nnfh);
    assert(pmeh.is_valid() && "no mutual edge");

    auto heh = m_mesh.halfedge_handle(pmeh, 0);
    if (m_mesh.face_handle(heh) != pfh)
        heh = m_mesh.halfedge_handle(pmeh, 1);
    assert(m_mesh.face_handle(heh) == pfh && "pmeh doesn't border pentagon?");

    // gather the vertices around the pentagon in order
    auto vhs = std::vector<OpenMesh::VertexHandle>(5, OpenMesh::VertexHandle{-1});
    for (size_t i = 0; i < 5; i++)
    {
        vhs[i] = m_mesh.to_vertex_handle(heh);
        heh = m_mesh.next_halfedge_handle(heh);
    }

    bool is0Ear = IsEar(m_mesh, vhs, 0);
    bool is4Ear = IsEar(m_mesh, vhs, 4);

    // convext quad, choose the ear that produces the best squareness
    if (is0Ear && is4Ear)
    {
        auto sq0 = CalculateSquarenessImpl({m_mesh.point(vhs[1]),
                                            m_mesh.point(vhs[2]),
                                            m_mesh.point(vhs[3]),
                                            m_mesh.point(vhs[4])});
        auto sq4 = CalculateSquarenessImpl({m_mesh.point(vhs[0]),
                                            m_mesh.point(vhs[1]),
                                            m_mesh.point(vhs[2]),
                                            m_mesh.point(vhs[3])});

        return sq0 > sq4 ? CutEar(pfh, vhs[0]) : CutEar(pfh, vhs[4]);
    }
    // neither is ear, difficult case
    else if (!is0Ear && !is4Ear)
    {
        auto ep = (m_mesh.point(vhs[0]) + m_mesh.point(vhs[2])) * 0.5f;
        auto vhep = m_mesh.add_vertex(ep);

        auto tp = (ep + m_mesh.point(vhs[1])) * 0.5f;
        auto vhtp = m_mesh.add_vertex(tp);

        m_mesh.delete_face(pfh, false); // pentagon

        m_mesh.add_face({vhep, vhtp, vhs[1], vhs[2]});   // degenerate quad 1
        m_mesh.add_face({vhep, vhs[0], vhs[1], vhtp});   // degenerate quad 2
        m_mesh.add_face({vhep, vhs[2], vhs[3], vhs[4]}); // smaller concave quad

        return m_mesh.add_face({vhs[0], vhep, vhs[4]}); // triangle
    }
    else if (is0Ear)
        return CutEar(pfh, vhs[0]);
    else if (is4Ear)
        return CutEar(pfh, vhs[4]);

    std::unreachable();
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
auto MeshConverter::CutEar(OpenMesh::FaceHandle fh, OpenMesh::VertexHandle vh) -> OpenMesh::FaceHandle
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

auto FindMutualEdge(const PolyMesh &mesh, OpenMesh::FaceHandle f1, OpenMesh::FaceHandle f2) -> OpenMesh::EdgeHandle
{
    for (auto eh : mesh.fe_range(f1))
    {
        if (mesh.is_boundary(eh))
            continue;

        auto heh0 = mesh.halfedge_handle(eh, 0);
        auto heh1 = mesh.halfedge_handle(eh, 1);

        auto fh0 = mesh.face_handle(heh0);
        auto fh1 = mesh.face_handle(heh1);

        if ((fh0 == f1 && fh1 == f2) || (fh0 == f2 && fh1 == f1))
            return eh;
    }

    return OpenMesh::EdgeHandle{-1};
};
auto GatherVerticesAroundEdge(const PolyMesh &mesh, OpenMesh::EdgeHandle eh) -> std::vector<OpenMesh::VertexHandle>
{
    auto heh0 = mesh.halfedge_handle(eh, 0);
    auto heh1 = mesh.halfedge_handle(eh, 1);

    std::vector<OpenMesh::VertexHandle> vertices;

    for (auto heh = heh0; mesh.next_halfedge_handle(heh) != heh0; heh = mesh.next_halfedge_handle(heh))
    {
        if (!heh.is_valid())
            break;

        vertices.push_back(mesh.to_vertex_handle(heh));
    }
    for (auto heh = heh1; mesh.next_halfedge_handle(heh) != heh1; heh = mesh.next_halfedge_handle(heh))
    {
        if (!heh.is_valid())
            break;

        vertices.push_back(mesh.to_vertex_handle(heh));
    }

    return vertices;
}
auto IsEar(const PolyMesh &mesh, const std::vector<OpenMesh::VertexHandle> &vhs, size_t earIndex) -> bool
{
    assert(earIndex < vhs.size() && "ear index out of bounds");

    auto isConvex = [](const OpenMesh::Vec3f &a,
                       const OpenMesh::Vec3f &b,
                       const OpenMesh::Vec3f &c,
                       const OpenMesh::Vec3f &N) -> bool
    {
        const auto e0 = b - a, e1 = c - b;
        const auto eps = e0.norm() * e1.norm() * 1e-6f;

        return dot(cross(e0, e1), N) > eps;
    };
    auto pointInTriangle = [](const OpenMesh::Vec3f &p,
                              const OpenMesh::Vec3f &a,
                              const OpenMesh::Vec3f &b,
                              const OpenMesh::Vec3f &c,
                              const OpenMesh::Vec3f &N)
    {
        float d1 = dot(cross(b - a, p - a), N);
        float d2 = dot(cross(c - b, p - b), N);
        float d3 = dot(cross(a - c, p - c), N);

        return d1 >= 0 && d2 >= 0 && d3 >= 0;
    };

    auto n = vhs.size();

    auto vhA = vhs[(earIndex + n - 1) % n];
    auto vhB = vhs[earIndex];
    auto vhC = vhs[(earIndex + 1) % n];

    const auto &a = mesh.point(vhA);
    const auto &b = mesh.point(vhB);
    const auto &c = mesh.point(vhC);

    // Newell's method for computing the normal of a polygon
    auto N = OpenMesh::Vec3f{0, 0, 0};
    for (size_t i = 0; i < n; i++)
    {
        const OpenMesh::Vec3f &p = mesh.point(vhs[i]);
        const OpenMesh::Vec3f &q = mesh.point(vhs[(i + 1) % n]);

        N[0] += (p[1] - q[1]) * (p[2] + q[2]);
        N[1] += (p[2] - q[2]) * (p[0] + q[0]);
        N[2] += (p[0] - q[0]) * (p[1] + q[1]);
    }
    N = N.normalize();

    if (!isConvex(a, b, c, N))
        return false;

    for (size_t i = 0; i < n; i++)
    {
        if (i == earIndex || i == (earIndex + n - 1) % n || i == (earIndex + 1) % n)
            continue;

        if (pointInTriangle(mesh.point(vhs[i]), a, b, c, N))
            return false;
    }

    return true;
}