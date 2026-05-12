#include "MeshSimplifier.hpp"

#include "Operations/Optimizing/VertexRotation/VertexRotation.hpp"
#include "Operations/Optimizing/EdgeRotation/EdgeRotation.hpp"
#include "Operations/Coarsening/EdgeCollapse/EdgeCollapse.hpp"
#include "Operations/Coarsening/DiagonalCollapse/DiagonalCollapse.hpp"

MeshSimplifier::MeshSimplifier(PolyMesh &topologyMesh)
	: m_mesh(topologyMesh), m_operationHeap(topologyMesh)
{
	m_mesh.request_vertex_status();
	m_mesh.request_edge_status();
	m_mesh.request_halfedge_status();
	m_mesh.request_face_status();

	m_mesh.add_property(m_vertexTimestamp, "vertex_timestamp");
	m_mesh.add_property(m_edgeTimestamp, "edge_timestamp");
	m_mesh.add_property(m_diagonalTimestamp, "diagonal_timestamp");

	for (auto vh : m_mesh.vertices())
	{
		m_mesh.property(m_vertexTimestamp, vh) = 0;
		AddVertexOperations(vh, 0);
	}
	for (auto eh : m_mesh.edges())
	{
		m_mesh.property(m_edgeTimestamp, eh) = 0;
		AddEdgeOperations(eh, 0);
	}
	for (auto heh : m_mesh.halfedges())
	{
		m_mesh.property(m_diagonalTimestamp, heh) = 0;
		AddDiagonalOperations(heh, 0);
	}

	PrintOperationHeap();
}

auto MeshSimplifier::TopologyMesh() -> PolyMesh &
{
	return m_mesh;
}

auto MeshSimplifier::Simplify(int32_t steps) -> void
{
	for (int32_t i = 0; i < steps; i++)
	{
		auto op = m_operationHeap.Pop();
		while (op)
		{
			op->Print();

			auto [vhs, ehs, dhs] = op->Execute();
			for (const auto vh : vhs)
				AddVertexOperations(vh, ++m_mesh.property(m_vertexTimestamp, vh));
			for (const auto eh : ehs)
				AddEdgeOperations(eh, ++m_mesh.property(m_edgeTimestamp, eh));
			for (const auto heh : dhs)
				AddDiagonalOperations(heh, ++m_mesh.property(m_diagonalTimestamp, heh));

			if (op->GetType() == OperationType::COARSENING)
				break;

			op = m_operationHeap.Pop();
		}
	}
}

// A trick to see inside a priority_queue without destroying it
template <class T, class S, class C>
const S &get_container(const std::priority_queue<T, S, C> &q)
{
	struct H : std::priority_queue<T, S, C>
	{
		static const S &get(const std::priority_queue<T, S, C> &q)
		{
			return q.*&H::c;
		}
	};
	return H::get(q);
}
auto MeshSimplifier::PrintOperationHeap() const -> void
{
#if 0
	std::cout << "Operation Heap:" << std::endl;
	const auto& container = get_container(m_operationHeap.GetInternalQueue()); // Assuming a getter
	for (const auto& op : container)
		op->Print();
#endif
}

auto MeshSimplifier::AddVertexOperations(OpenMesh::VertexHandle vh, int32_t timestamp) -> void
{
	m_operationHeap.Push(std::make_unique<VertexRotation>(m_mesh, vh, timestamp));
}
auto MeshSimplifier::AddEdgeOperations(OpenMesh::EdgeHandle eh, int32_t timestamp) -> void
{
	auto heh0 = m_mesh.halfedge_handle(eh, 0);
	auto heh1 = m_mesh.halfedge_handle(eh, 1);

	m_operationHeap.Push(std::make_unique<EdgeRotation>(m_mesh, heh0, true, timestamp));
	m_operationHeap.Push(std::make_unique<EdgeRotation>(m_mesh, heh0, false, timestamp));

	m_operationHeap.Push(std::make_unique<EdgeCollapse>(m_mesh, heh0, timestamp));
	m_operationHeap.Push(std::make_unique<EdgeCollapse>(m_mesh, heh1, timestamp));
}
auto MeshSimplifier::AddDiagonalOperations(OpenMesh::HalfedgeHandle heh, int32_t timestamp) -> void
{
	m_operationHeap.Push(std::make_unique<DiagonalCollapse>(m_mesh, heh, timestamp));
}

MeshSimplifier::OperationHeap::OperationHeap(PolyMesh &mesh)
	: m_mesh(mesh)
{
}

auto MeshSimplifier::OperationHeap::Push(std::unique_ptr<Operation> op) -> void
{
	m_operationHeap.push(std::move(op));
}

auto MeshSimplifier::OperationHeap::Pop() -> std::unique_ptr<Operation>
{
	while (!m_operationHeap.empty())
	{
		auto op = std::move(const_cast<std::unique_ptr<Operation> &>(m_operationHeap.top()));
		m_operationHeap.pop();
		if (op->IsValid())
			return op;
	}
	return nullptr;
}