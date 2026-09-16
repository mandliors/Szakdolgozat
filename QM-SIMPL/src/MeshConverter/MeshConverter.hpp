#pragma once

#include "MeshRenderer/MeshRenderer.hpp"

#include "OpenMesh/Core/IO/MeshIO.hh"
#include "OpenMesh/Core/Mesh/PolyMesh_ArrayKernelT.hh"
#include "nanoflann.hpp"

#include "Operations/Operation.hpp"

#include <memory>
#include <queue>

using PolyMesh = OpenMesh::PolyMesh_ArrayKernelT<>;

class MeshConverter
{
public:
    explicit MeshConverter(PolyMesh &mesh);

    auto TopologyMesh() -> PolyMesh &;

    auto Execute() -> void;

private:
    auto DissolveEdges() -> void;
    auto MergeRemainingTriangles() -> void;

    auto DissolveEdge(OpenMesh::EdgeHandle eh) -> OpenMesh::FaceHandle;
    auto SetEdgeSquarenessValues() -> void;
    auto SelectEdges() -> void;
    auto FlagEdges() -> void;
    auto CalculateEdgeSquareness(OpenMesh::EdgeHandle eh) -> float;
    auto CalculateSquarenessImpl(const std::array<OpenMesh::Vec3f, 4> &vtx) -> float;

    auto SolveTriangle(OpenMesh::FaceHandle fh) -> void;
    auto FindPathToNearestTriangle(OpenMesh::FaceHandle fh) -> std::vector<OpenMesh::FaceHandle>;
    auto CutTriangle(OpenMesh::FaceHandle fh, OpenMesh::VertexHandle vh) -> OpenMesh::FaceHandle;

private:
    PolyMesh &m_mesh;

    OpenMesh::EPropHandleT<float> m_squareness;
    OpenMesh::EPropHandleT<bool> m_selected;
    OpenMesh::EPropHandleT<bool> m_flagged;
};