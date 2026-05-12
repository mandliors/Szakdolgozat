#include "EdgeRotation.hpp"

static auto CalculateProfitability(const PolyMesh& mesh, OpenMesh::HalfedgeHandle heh, bool ccw) -> float;

EdgeRotation::EdgeRotation(PolyMesh& mesh, OpenMesh::HalfedgeHandle heh, bool ccw, int32_t timestamp)
	: Operation(CalculateProfitability(mesh, heh, ccw), timestamp), m_mesh(mesh), m_heh(heh), m_ccw(ccw) {
	m_mesh.get_property_handle(m_timestampHandle, "edge_timestamp");
}

auto EdgeRotation::IsValid() const -> bool
{
	if (!m_mesh.is_valid_handle(m_heh) || m_mesh.status(m_heh).deleted())
		return false;

	if (m_mesh.property(m_timestampHandle, m_mesh.edge_handle(m_heh)) != m_timestamp)
		return false;

	return true;
}
auto EdgeRotation::Execute()->std::tuple<
	std::unordered_set<PolyMesh::VertexHandle, MeshHandleHasher>,
	std::unordered_set<PolyMesh::EdgeHandle, MeshHandleHasher>,
	std::unordered_set<PolyMesh::HalfedgeHandle, MeshHandleHasher>>
{
	if (m_mesh.is_boundary(m_heh)) return { {}, {}, {} };

	auto heh0 = m_heh;
	auto heh1 = m_mesh.opposite_halfedge_handle(m_heh);

	auto fh0 = m_mesh.face_handle(heh0);
	auto fh1 = m_mesh.face_handle(heh1);

	// 0 -- 3 is the given edge
	// 
	// 1 -- 0 -- 5
	// |    |    |
	// 2 -- 3 -- 4
	auto v0 = m_mesh.to_vertex_handle(heh0);
	auto v3 = m_mesh.to_vertex_handle(heh1);

	auto v1 = m_mesh.to_vertex_handle(m_mesh.next_halfedge_handle(heh0));
	auto v2 = m_mesh.to_vertex_handle(m_mesh.next_halfedge_handle(m_mesh.next_halfedge_handle(heh0)));

	auto v4 = m_mesh.to_vertex_handle(m_mesh.next_halfedge_handle(heh1));
	auto v5 = m_mesh.to_vertex_handle(m_mesh.next_halfedge_handle(m_mesh.next_halfedge_handle(heh1)));

	m_mesh.delete_face(fh0, false);
	m_mesh.delete_face(fh1, false);

	std::vector<PolyMesh::VertexHandle> nf1;
	std::vector<PolyMesh::VertexHandle> nf2;

	if (m_ccw)
	{
		nf1.insert(nf1.end(), { v1, v2, v3, v4 });
		nf2.insert(nf2.end(), { v4, v5, v0, v1 });
	}
	else
	{
		nf1.insert(nf1.end(), { v2, v3, v4, v5 });
		nf2.insert(nf2.end(), { v5, v0, v1, v2 });
	}

	auto f1 = m_mesh.add_face(nf1);
	auto f2 = m_mesh.add_face(nf2);


	std::unordered_set<OpenMesh::VertexHandle, MeshHandleHasher> affectedVhs;
	std::unordered_set<OpenMesh::EdgeHandle, MeshHandleHasher> affectedEhs;
	std::unordered_set<OpenMesh::HalfedgeHandle, MeshHandleHasher> affectedDhs;

	for (auto heh : m_mesh.fh_range(f1))
	{
		affectedVhs.insert(m_mesh.to_vertex_handle(heh));
		affectedEhs.insert(m_mesh.edge_handle(heh));
		affectedDhs.insert(heh);
	}
	for (auto heh : m_mesh.fh_range(f2))
	{
		affectedVhs.insert(m_mesh.to_vertex_handle(heh));
		affectedEhs.insert(m_mesh.edge_handle(heh));
		affectedDhs.insert(heh);
	}
	return { affectedVhs, affectedEhs, affectedDhs };
}

static auto CalculateProfitability(const PolyMesh& mesh, OpenMesh::HalfedgeHandle heh, bool ccw) -> float
{
	auto heh0 = heh;
	auto heh1 = mesh.opposite_halfedge_handle(heh);

	// 0 -- 3 is the given edge
	// 
	// 1 -- 0 -- 5
	// |    |    |
	// 2 -- 3 -- 4
	const auto& v0 = mesh.point(mesh.to_vertex_handle(heh0));
	const auto& v3 = mesh.point(mesh.to_vertex_handle(heh1));

	const auto& v1 = mesh.point(mesh.to_vertex_handle(mesh.next_halfedge_handle(heh0)));
	const auto& v2 = mesh.point(mesh.to_vertex_handle(mesh.next_halfedge_handle(mesh.next_halfedge_handle(heh0))));

	const auto& v4 = mesh.point(mesh.to_vertex_handle(mesh.next_halfedge_handle(heh1)));
	const auto& v5 = mesh.point(mesh.to_vertex_handle(mesh.next_halfedge_handle(mesh.next_halfedge_handle(heh1))));

	float edgeProfit, diag1Profit, diag2Profit;

	if (ccw)
	{
		auto oldEdge = (v3 - v0).length();
		auto newEdge = (v4 - v1).length();
		edgeProfit = oldEdge - newEdge;

		auto oldDiag1 = (v2 - v0).length();
		auto newDiag1 = (v2 - v4).length();
		diag1Profit = oldDiag1 - newDiag1;

		auto oldDiag2 = (v5 - v3).length();
		auto newDiag2 = (v5 - v1).length();
		diag2Profit = oldDiag2 - newDiag2;
	}
	else
	{
		auto oldEdge = (v3 - v0).length();
		auto newEdge = (v2 - v5).length();
		edgeProfit = oldEdge - newEdge;

		auto oldDiag1 = (v4 - v0).length();
		auto newDiag1 = (v4 - v2).length();
		diag1Profit = oldDiag1 - newDiag1;

		auto oldDiag2 = (v1 - v3).length();
		auto newDiag2 = (v1 - v5).length();
		diag2Profit = oldDiag2 - newDiag2;
	}

	return edgeProfit + diag1Profit + diag2Profit;
}
