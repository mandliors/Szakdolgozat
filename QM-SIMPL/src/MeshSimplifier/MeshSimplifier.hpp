#pragma once

#include "MeshRenderer/MeshRenderer.hpp"

#include "OpenMesh/Core/IO/MeshIO.hh"
#include "OpenMesh/Core/Mesh/PolyMesh_ArrayKernelT.hh"

#include "Operations/Operation.hpp"

#include <memory>
#include <queue>

using PolyMesh = OpenMesh::PolyMesh_ArrayKernelT<>;

class MeshSimplifier
{
public:
	explicit MeshSimplifier(PolyMesh &topologyMesh);

	auto TopologyMesh() -> PolyMesh &;

	auto Simplify(int32_t steps) -> void;

public:
	auto PrintOperationHeap() const -> void;

private:
	auto AddVertexOperations(OpenMesh::VertexHandle vh, int32_t timestamp) -> void;
	auto AddEdgeOperations(OpenMesh::EdgeHandle eh, int32_t timestamp) -> void;
	auto AddDiagonalOperations(OpenMesh::HalfedgeHandle heh, int32_t timestamp) -> void;

private:
	class OperationHeap
	{
	public:
		explicit OperationHeap(PolyMesh &mesh);
		auto Push(std::unique_ptr<Operation> op) -> void;
		auto Pop() -> std::unique_ptr<Operation>;

	private:
		class OperationComparator
		{
		public:
			auto operator()(
				const std::unique_ptr<Operation> &a,
				const std::unique_ptr<Operation> &b) const -> bool
			{
				return *a > *b;
			}
		};

	public:
		auto GetInternalQueue() const -> const std::priority_queue<
			std::unique_ptr<Operation>,
			std::vector<std::unique_ptr<Operation>>,
			OperationComparator> &
		{
			return m_operationHeap;
		}

	private:
		PolyMesh &m_mesh;
		std::priority_queue<
			std::unique_ptr<Operation>,
			std::vector<std::unique_ptr<Operation>>,
			OperationComparator>
			m_operationHeap;
		std::vector<double> m_minLengths;
	};

private:
	PolyMesh &m_mesh;
	OperationHeap m_operationHeap;

	OpenMesh::VPropHandleT<int32_t> m_vertexTimestamp;
	OpenMesh::EPropHandleT<int32_t> m_edgeTimestamp;
	OpenMesh::HPropHandleT<int32_t> m_diagonalTimestamp;
};