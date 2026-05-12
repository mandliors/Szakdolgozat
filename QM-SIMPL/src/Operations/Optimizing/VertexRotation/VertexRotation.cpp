#include "VertexRotation.hpp"

static auto CalculateProfitability(const PolyMesh& mesh, OpenMesh::VertexHandle vh) -> float;

VertexRotation::VertexRotation(PolyMesh& mesh, OpenMesh::VertexHandle vh, int32_t timestamp)
	: Operation(CalculateProfitability(mesh, vh), timestamp), m_mesh(mesh), m_vh(vh) {
	m_mesh.get_property_handle(m_timestampHandle, "vertex_rotation_timestamp");
}

auto VertexRotation::IsValid() const -> bool
{
	if (!m_mesh.is_valid_handle(m_vh) || m_mesh.status(m_vh).deleted())
		return false;

	if (m_mesh.property(m_timestampHandle, m_vh) != m_timestamp)
		return false;

	return true;
}
auto VertexRotation::Execute()->std::tuple<
	std::unordered_set<PolyMesh::VertexHandle, MeshHandleHasher>,
	std::unordered_set<PolyMesh::EdgeHandle, MeshHandleHasher>,
	std::unordered_set<PolyMesh::HalfedgeHandle, MeshHandleHasher>>
{
	if (m_mesh.is_boundary(m_vh)) return { {}, {}, {} };

	std::vector<PolyMesh::VertexHandle> neighbors;
	for (auto voh_it = m_mesh.voh_iter(m_vh); voh_it.is_valid(); ++voh_it)
	{
		neighbors.push_back(m_mesh.to_vertex_handle(m_mesh.next_halfedge_handle(*voh_it)));
		neighbors.push_back(m_mesh.to_vertex_handle(*voh_it));
	}

	std::vector<PolyMesh::FaceHandle> oldFaces;
	for (auto fh : m_mesh.vf_range(m_vh))
		oldFaces.push_back(fh);
	for (auto fh : oldFaces)
		m_mesh.delete_face(fh, false);

	auto n = neighbors.size();
	for (size_t i = 0; i < n; i += 2)
	{
		std::vector<PolyMesh::VertexHandle> nf =
		{
			m_vh,
			neighbors[(i + 2) % n],
			neighbors[(i + 1) % n],
			neighbors[i],
		};
		m_mesh.add_face(nf);
	}


	std::unordered_set<OpenMesh::VertexHandle, MeshHandleHasher> affectedVhs{ m_vh };
	std::unordered_set<OpenMesh::EdgeHandle, MeshHandleHasher> affectedEhs;
	std::unordered_set<OpenMesh::HalfedgeHandle, MeshHandleHasher> affectedDhs;

	for (auto voh_it = m_mesh.voh_iter(m_vh); voh_it.is_valid(); ++voh_it)
	{
		auto heh0 = *voh_it;
		auto heh1 = m_mesh.next_halfedge_handle(heh0);
		auto heh2 = m_mesh.next_halfedge_handle(heh1);
		auto heh3 = m_mesh.next_halfedge_handle(heh2);

		affectedVhs.insert(m_mesh.to_vertex_handle(heh0));
		affectedVhs.insert(m_mesh.to_vertex_handle(heh1));

		affectedEhs.insert(m_mesh.edge_handle(heh0));
		affectedEhs.insert(m_mesh.edge_handle(heh1));
		affectedEhs.insert(m_mesh.edge_handle(heh2));

		affectedDhs.insert(heh0);
		affectedDhs.insert(heh1);
		affectedDhs.insert(heh2);
		affectedDhs.insert(heh3);
	}

	return { affectedVhs, affectedEhs, affectedDhs };
}

static auto CalculateProfitability(const PolyMesh& mesh, OpenMesh::VertexHandle vh) -> float
{
	float profit = 0.0f;
	const auto& p0 = mesh.point(vh);

	for (auto voh_it = mesh.cvoh_iter(vh); voh_it.is_valid(); ++voh_it)
	{
		auto vh1 = mesh.to_vertex_handle(*voh_it);
		auto vh2 = mesh.to_vertex_handle(mesh.next_halfedge_handle(*voh_it));

		auto p1 = mesh.point(vh1);
		auto p2 = mesh.point(vh2);

		float edgeLength = (p1 - p0).length();
		float diagLength = (p2 - p0).length();

		profit += edgeLength - diagLength;
	}
	return profit;
}