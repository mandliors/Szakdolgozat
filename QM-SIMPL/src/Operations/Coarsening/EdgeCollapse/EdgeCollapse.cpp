#include "EdgeCollapse.hpp"
#include "../DiagonalCollapse/DiagonalCollapse.hpp"
#include "../../Optimizing/VertexRotation/VertexRotation.hpp"

static auto CalculateProfitability(const PolyMesh &mesh, OpenMesh::HalfedgeHandle heh) -> float;
static OpenMesh::HalfedgeHandle GetHalfedgeFromFaceAndVertex(const PolyMesh &mesh, OpenMesh::FaceHandle fh, OpenMesh::VertexHandle vh);

EdgeCollapse::EdgeCollapse(PolyMesh &mesh, OpenMesh::HalfedgeHandle heh, int32_t timestamp)
	: Operation(CalculateProfitability(mesh, heh), timestamp), m_mesh(mesh), m_heh(heh)
{
	m_mesh.get_property_handle(m_timestampHandle, "edge_timestamp");
}

auto EdgeCollapse::IsValid() const -> bool
{
	if (!m_mesh.is_valid_handle(m_heh) || m_mesh.status(m_heh).deleted())
		return false;

	if (m_mesh.property(m_timestampHandle, m_mesh.edge_handle(m_heh)) != m_timestamp)
		return false;

	return true;
}
auto EdgeCollapse::Execute() -> std::tuple<
	std::unordered_set<PolyMesh::VertexHandle, MeshHandleHasher>,
	std::unordered_set<PolyMesh::EdgeHandle, MeshHandleHasher>,
	std::unordered_set<PolyMesh::HalfedgeHandle, MeshHandleHasher>>
{
	auto findFaceFromDiagonal = [&](OpenMesh::VertexHandle v0, OpenMesh::VertexHandle v1) -> OpenMesh::FaceHandle
	{
		for (auto fh : m_mesh.vf_range(v0))
			for (auto vh : m_mesh.fv_range(fh))
				if (vh == v1)
					return fh;

		return OpenMesh::FaceHandle{-1};
	};

	auto vh0 = m_mesh.from_vertex_handle(m_heh);
	auto vh1 = m_mesh.to_vertex_handle(m_heh);

	std::unordered_set<OpenMesh::VertexHandle, MeshHandleHasher> affectedVhs;
	std::unordered_set<OpenMesh::EdgeHandle, MeshHandleHasher> affectedEhs;
	std::unordered_set<OpenMesh::HalfedgeHandle, MeshHandleHasher> affectedDhs;

	// gather all affected vertices
	for (auto voh_it = m_mesh.voh_iter(vh0); voh_it.is_valid(); ++voh_it)
	{
		auto heh0 = m_mesh.next_halfedge_handle(*voh_it);
		auto heh1 = m_mesh.next_halfedge_handle(heh0);

		affectedVhs.insert(m_mesh.to_vertex_handle(heh0));
		affectedVhs.insert(m_mesh.to_vertex_handle(heh1));
	}
	for (auto voh_it = m_mesh.voh_iter(vh1); voh_it.is_valid(); ++voh_it)
	{
		auto heh0 = m_mesh.next_halfedge_handle(*voh_it);
		auto heh1 = m_mesh.next_halfedge_handle(heh0);

		affectedVhs.insert(m_mesh.to_vertex_handle(heh0));
		affectedVhs.insert(m_mesh.to_vertex_handle(heh1));
	}

	auto [vrVhs, vrEhs, vrDhs] = VertexRotation{m_mesh, vh0, 0}.Execute();

	auto fh = findFaceFromDiagonal(vh0, vh1);
	if (!fh.is_valid())
		return {{}, {}, {}}; // rotation didn't produce the expected diagonal

	auto heh = GetHalfedgeFromFaceAndVertex(m_mesh, fh, vh0);
	if (heh.idx() == -1)
	{
		std::cout << heh << std::endl;
	}

	auto [dcVhs, dcEhs, dcDhs] = DiagonalCollapse{m_mesh, heh, 0}.Execute();
	if (dcVhs.empty())
		return {{}, {}, {}}; // diagonal collapse's own precheck rejected it too

	// merge in everything touched by both sub-operations
	affectedVhs.insert(vrVhs.begin(), vrVhs.end());
	affectedVhs.insert(dcVhs.begin(), dcVhs.end());
	affectedEhs.insert(vrEhs.begin(), vrEhs.end());
	affectedEhs.insert(dcEhs.begin(), dcEhs.end());
	affectedDhs.insert(vrDhs.begin(), vrDhs.end());
	affectedDhs.insert(dcDhs.begin(), dcDhs.end());

	// vh0 and vh1 no longer exist, remove them from the affected set
	affectedVhs.erase(vh0);
	affectedVhs.erase(vh1);

	// gather all affected edges and diagonals
	for (auto vh : affectedVhs)
	{
		for (auto ve_iter = m_mesh.ve_iter(vh); ve_iter.is_valid(); ++ve_iter)
			affectedEhs.insert(*ve_iter);
		for (auto voh_iter = m_mesh.voh_iter(vh); voh_iter.is_valid(); ++voh_iter)
			affectedDhs.insert(*voh_iter);
	}

	return {affectedVhs, affectedEhs, affectedDhs};
}

static auto CalculateProfitability(const PolyMesh &mesh, OpenMesh::HalfedgeHandle heh) -> float
{
	const auto &p0 = mesh.point(mesh.from_vertex_handle(heh));
	const auto &p1 = mesh.point(mesh.to_vertex_handle(heh));
	float lengthSqr = (p1 - p0).sqrnorm();

	return 1.0f / lengthSqr;
}
static OpenMesh::HalfedgeHandle GetHalfedgeFromFaceAndVertex(const PolyMesh &mesh, OpenMesh::FaceHandle fh, OpenMesh::VertexHandle vh)
{
	for (auto heh : mesh.fh_range(fh))
		if (mesh.from_vertex_handle(heh) == vh)
			return heh;

	return OpenMesh::HalfedgeHandle{-1};
}