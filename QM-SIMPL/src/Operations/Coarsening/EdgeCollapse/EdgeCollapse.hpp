#pragma once

#include "../../Operation.hpp"

using PolyMesh = OpenMesh::PolyMesh_ArrayKernelT<>;

class EdgeCollapse : public Operation
{
public:
	EdgeCollapse(PolyMesh& mesh, OpenMesh::HalfedgeHandle heh, int32_t timestamp);

	auto GetType() const -> OperationType override { return OperationType::COARSENING; }

	auto IsValid() const -> bool override;
	auto Execute() -> std::tuple<
		std::unordered_set<PolyMesh::VertexHandle, MeshHandleHasher>,
		std::unordered_set<PolyMesh::EdgeHandle, MeshHandleHasher>,
		std::unordered_set<PolyMesh::HalfedgeHandle, MeshHandleHasher>> override;

	auto Print() const -> void override
	{
		std::cout << "   EdgeCollapse";
		std::cout << " with profitability" << GetProfitability();
		std::cout << " and timestamp " << GetTimestamp();
		std::cout << std::endl;
	}

private:
	PolyMesh& m_mesh;
	OpenMesh::HalfedgeHandle m_heh;
	OpenMesh::EPropHandleT<int32_t> m_timestampHandle;
};