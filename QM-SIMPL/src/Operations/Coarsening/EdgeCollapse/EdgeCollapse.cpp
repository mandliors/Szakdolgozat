#include "EdgeCollapse.hpp"
#include "../DiagonalCollapse/DiagonalCollapse.hpp"
#include "../../Optimizing/VertexRotation/VertexRotation.hpp"

static auto CalculateProfitability(const PolyMesh& mesh, OpenMesh::HalfedgeHandle heh) -> float;
static OpenMesh::HalfedgeHandle GetHalfedgeFromFaceAndVertex(const PolyMesh& mesh, OpenMesh::FaceHandle fh, OpenMesh::VertexHandle vh);

EdgeCollapse::EdgeCollapse(PolyMesh& mesh, OpenMesh::HalfedgeHandle heh, int32_t timestamp)
	: Operation(CalculateProfitability(mesh, heh), timestamp), m_mesh(mesh), m_heh(heh) {
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
auto EdgeCollapse::Execute()->std::tuple<
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

			return OpenMesh::FaceHandle{ -1 };
		};

	auto vh0 = m_mesh.from_vertex_handle(m_heh);
	auto vh1 = m_mesh.to_vertex_handle(m_heh);

	VertexRotation{ m_mesh, vh0, 0 }.Execute();
	DiagonalCollapse{
		m_mesh,
		GetHalfedgeFromFaceAndVertex(m_mesh, findFaceFromDiagonal(vh0, vh1), vh0),
		0
	}.Execute();


	std::unordered_set<OpenMesh::VertexHandle, MeshHandleHasher> affectedVhs{ vh1 };
	std::unordered_set<OpenMesh::EdgeHandle, MeshHandleHasher> affectedEhs;
	std::unordered_set<OpenMesh::HalfedgeHandle, MeshHandleHasher> affectedDhs;

	for (auto voh_it = m_mesh.voh_begin(vh1); voh_it != m_mesh.voh_end(vh1); ++voh_it)
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

static auto CalculateProfitability(const PolyMesh& mesh, OpenMesh::HalfedgeHandle heh) -> float
{
	const auto& p0 = mesh.point(mesh.from_vertex_handle(heh));
	const auto& p1 = mesh.point(mesh.to_vertex_handle(heh));
	float lengthSqr = (p1 - p0).sqrnorm();

	return 1.0f / lengthSqr;
}
static OpenMesh::HalfedgeHandle GetHalfedgeFromFaceAndVertex(const PolyMesh& mesh, OpenMesh::FaceHandle fh, OpenMesh::VertexHandle vh)
{
	for (auto heh : mesh.fh_range(fh))
		if (mesh.from_vertex_handle(heh) == vh)
			return heh;

	return OpenMesh::HalfedgeHandle{ -1 };
}