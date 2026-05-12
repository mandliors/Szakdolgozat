#include "DiagonalCollapse.hpp"

#include <ranges>

static auto CalculateProfitability(const PolyMesh& mesh, OpenMesh::HalfedgeHandle heh) -> float;

DiagonalCollapse::DiagonalCollapse(PolyMesh& mesh, OpenMesh::HalfedgeHandle heh, int32_t timestamp)
	: Operation(CalculateProfitability(mesh, heh), timestamp), m_mesh(mesh), m_heh(heh) {
	m_mesh.get_property_handle(m_timestampHandle, "diagonal_timestamp");
}

auto DiagonalCollapse::IsValid() const -> bool
{
	if (!m_mesh.is_valid_handle(m_heh) || m_mesh.status(m_heh).deleted())
		return false;

	if (m_mesh.property(m_timestampHandle, m_heh) != m_timestamp)
		return false;

	return true;
}
auto DiagonalCollapse::Execute()->std::tuple<
	std::unordered_set<PolyMesh::VertexHandle, MeshHandleHasher>,
	std::unordered_set<PolyMesh::EdgeHandle, MeshHandleHasher>,
	std::unordered_set<PolyMesh::HalfedgeHandle, MeshHandleHasher>>
{
	const auto fh = m_mesh.face_handle(m_heh);
	const auto vh0 = m_mesh.from_vertex_handle(m_heh);
	const auto vh1 = m_mesh.to_vertex_handle(m_mesh.next_halfedge_handle(m_heh));

	auto vhs = std::vector<OpenMesh::VertexHandle>{};
	auto addNewFacesData = [&](OpenMesh::VertexHandle vh)
		{
			for (auto voh_it = m_mesh.voh_iter(vh); voh_it.is_valid(); ++voh_it)
			{
				if (m_mesh.face_handle(*voh_it) == fh)
					continue;

				auto voh = *voh_it;
				for (size_t i = 1; i <= 3; i++)
				{
					vhs.push_back(m_mesh.to_vertex_handle(voh));
					voh = m_mesh.next_halfedge_handle(voh);
				}
			}
		};

	auto newV = (m_mesh.point(vh0) + m_mesh.point(vh1)) * 0.5f;
	addNewFacesData(vh0);
	addNewFacesData(vh1);

	m_mesh.delete_vertex(vh0, false);
	m_mesh.delete_vertex(vh1, false);

	std::vector<OpenMesh::FaceHandle> newFhs; newFhs.reserve(vhs.size() / 3);
	auto newVh = m_mesh.add_vertex(newV);
	for (auto chunk : vhs | std::views::chunk(3))
		newFhs.push_back(m_mesh.add_face(newVh, chunk[0], chunk[1], chunk[2]));


	/*std::vector<OpenMesh::VertexHandle> neighbors;
	for (auto v : m_mesh.vv_range(newVh))
		neighbors.push_back(v);

	for (auto n : neighbors)
		if (m_mesh.valence(n) == 1)
			RemoveSinglet(m_mesh, n);*/

	std::unordered_set<OpenMesh::VertexHandle, MeshHandleHasher> affectedVhs;
	std::unordered_set<OpenMesh::EdgeHandle, MeshHandleHasher> affectedEhs;
	std::unordered_set<OpenMesh::HalfedgeHandle, MeshHandleHasher> affectedDhs;

	for (auto fh : newFhs)
	{
		for (auto vh : m_mesh.fv_range(fh)) affectedVhs.insert(vh);
		for (auto feh : m_mesh.fe_range(fh)) affectedEhs.insert(feh);
		for (auto heh : m_mesh.fh_range(fh)) affectedDhs.insert(heh);
	}

	return { affectedVhs, affectedEhs, affectedDhs };
}

static auto CalculateProfitability(const PolyMesh& mesh, OpenMesh::HalfedgeHandle heh) -> float
{
	const auto& p0 = mesh.point(mesh.from_vertex_handle(heh));
	const auto& p1 = mesh.point(mesh.to_vertex_handle(mesh.next_halfedge_handle(heh)));
	float lengthSqr = (p1 - p0).sqrnorm();

	return 2.0f / lengthSqr; // 1 / (lengthSqr / 2): length has to be divided by sqrt(2)
}