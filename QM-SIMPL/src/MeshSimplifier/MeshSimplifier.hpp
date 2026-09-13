#pragma once

#include "MeshRenderer/MeshRenderer.hpp"

#include "OpenMesh/Core/IO/MeshIO.hh"
#include "OpenMesh/Core/Mesh/PolyMesh_ArrayKernelT.hh"
#include "nanoflann.hpp"

#include "Operations/Operation.hpp"

#include <memory>
#include <queue>

using PolyMesh = OpenMesh::PolyMesh_ArrayKernelT<>;

class MeshSimplifier
{
	struct FaceCentroidCloud;
	using KDTree = nanoflann::KDTreeSingleIndexAdaptor<
		nanoflann::L2_Simple_Adaptor<double, FaceCentroidCloud>,
		FaceCentroidCloud,
		3>;

public:
	explicit MeshSimplifier(const PolyMesh &originalMesh, PolyMesh &topologyMesh);

	auto TopologyMesh() -> PolyMesh &;

	auto Simplify(int32_t steps) -> void;

public:
	auto PrintOperationHeap() const -> void;

private:
	auto AddVertexOperations(OpenMesh::VertexHandle vh, int32_t timestamp) -> void;
	auto AddEdgeOperations(OpenMesh::EdgeHandle eh, int32_t timestamp) -> void;
	auto AddDiagonalOperations(OpenMesh::HalfedgeHandle heh, int32_t timestamp) -> void;

	auto BuildCloud() -> void;

public:
	auto ProjectVertex(OpenMesh::VertexHandle vh) -> std::tuple<
		std::unordered_set<PolyMesh::VertexHandle, MeshHandleHasher>,
		std::unordered_set<PolyMesh::EdgeHandle, MeshHandleHasher>,
		std::unordered_set<PolyMesh::HalfedgeHandle, MeshHandleHasher>>;

private:
	auto ClosestFace(const OpenMesh::Vec3f &p, size_t candidateCount = 10) const -> OpenMesh::FaceHandle;

private:
	struct FaceCentroidCloud
	{
		auto kdtree_get_point_count() const -> size_t { return faces.size(); }
		auto kdtree_get_pt(const size_t idx, const size_t dim) const -> float
		{
			return faces[idx].first[dim];
		}

		template <class BBOX>
		auto kdtree_get_bbox(BBOX &bbox) const -> bool { return false; }

		std::vector<std::pair<OpenMesh::Vec3f, OpenMesh::FaceHandle>> faces;
	};

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
				return *b > *a;
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
	PolyMesh m_originalMesh;

	OperationHeap m_operationHeap;

	std::unique_ptr<FaceCentroidCloud> m_faceCentroidCloud;
	std::unique_ptr<KDTree> m_centroidTree;

	OpenMesh::VPropHandleT<int32_t> m_vertexTimestamp;
	OpenMesh::EPropHandleT<int32_t> m_edgeTimestamp;
	OpenMesh::HPropHandleT<int32_t> m_diagonalTimestamp;
};