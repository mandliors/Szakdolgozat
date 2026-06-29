#pragma once

#include "../../Operation.hpp"

#include "OpenMesh/Core/IO/MeshIO.hh"
#include "OpenMesh/Core/Mesh/PolyMesh_ArrayKernelT.hh"

#include <iostream>

using PolyMesh = OpenMesh::PolyMesh_ArrayKernelT<>;

class VertexRotation : public Operation
{
public:
	VertexRotation(PolyMesh& mesh, OpenMesh::VertexHandle vh, int32_t timestamp);

	auto GetType() const -> OperationType override { return OperationType::OPTIMIZING; }

	auto IsValid() const -> bool override;
	auto Execute() -> std::tuple<
		std::unordered_set<PolyMesh::VertexHandle, MeshHandleHasher>,
		std::unordered_set<PolyMesh::EdgeHandle, MeshHandleHasher>,
		std::unordered_set<PolyMesh::HalfedgeHandle, MeshHandleHasher>> override;

	auto Print() const -> void override
	{
		std::cout << "   VertexRotation";
		std::cout << " with profitability" << GetProfitability();
		std::cout << " and timestamp " << GetTimestamp();
		std::cout << std::endl;
	}

private:
	PolyMesh& m_mesh;
	OpenMesh::VertexHandle m_vh;
	OpenMesh::VPropHandleT<int32_t> m_timestampHandle;
};