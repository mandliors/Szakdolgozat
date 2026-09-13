#include "VertexRotation.hpp"

static auto CalculateProfitability(const PolyMesh &mesh, OpenMesh::VertexHandle vh) -> float;

VertexRotation::VertexRotation(PolyMesh &mesh, OpenMesh::VertexHandle vh, int32_t timestamp)
	: Operation(CalculateProfitability(mesh, vh), timestamp), m_mesh(mesh), m_vh(vh)
{
	m_mesh.get_property_handle(m_timestampHandle, "vertex_timestamp");
}

auto VertexRotation::IsValid() const -> bool
{
	if (!m_mesh.is_valid_handle(m_vh) || m_mesh.status(m_vh).deleted())
		return false;

	if (m_mesh.property(m_timestampHandle, m_vh) != m_timestamp)
		return false;

	return true;
}
auto VertexRotation::Execute() -> std::tuple<
	std::unordered_set<PolyMesh::VertexHandle, MeshHandleHasher>,
	std::unordered_set<PolyMesh::EdgeHandle, MeshHandleHasher>,
	std::unordered_set<PolyMesh::HalfedgeHandle, MeshHandleHasher>>
{
	if (m_mesh.is_boundary(m_vh))
		return {{}, {}, {}};

	std::vector<PolyMesh::VertexHandle> neighbors;
	for (auto voh_it = m_mesh.voh_iter(m_vh); voh_it.is_valid(); ++voh_it)
	{
		neighbors.push_back(m_mesh.to_vertex_handle(m_mesh.next_halfedge_handle(*voh_it)));
		neighbors.push_back(m_mesh.to_vertex_handle(*voh_it));
	}

	auto n = neighbors.size();

	// precheck: verify every prospective new edge is safe to introduce
	for (size_t i = 0; i < n; i += 2)
	{
		auto a = neighbors[(i + 2) % n];
		auto b = neighbors[(i + 1) % n];
		auto c = neighbors[i];

		if (m_mesh.find_halfedge(a, b).is_valid() || m_mesh.find_halfedge(b, c).is_valid())
			return {{}, {}, {}};
	}

	std::vector<PolyMesh::FaceHandle> oldFaces;
	for (auto fh : m_mesh.vf_range(m_vh))
		oldFaces.push_back(fh);
	for (auto fh : oldFaces)
		m_mesh.delete_face(fh, false);

	std::vector<PolyMesh::FaceHandle> newFaces;
	newFaces.reserve(n / 2);

	bool failed = false;
	for (size_t i = 0; i < n; i += 2)
	{
		std::vector<PolyMesh::VertexHandle> nf =
			{
				m_vh,
				neighbors[(i + 2) % n],
				neighbors[(i + 1) % n],
				neighbors[i],
		};
		auto fh = m_mesh.add_face(nf);
		if (!fh.is_valid())
		{
			failed = true;
			break;
		}
		newFaces.push_back(fh);
	}

	if (failed)
	{
		for (auto f : newFaces)
			m_mesh.delete_face(f, false);

		bool restoreFailed = false;
		std::vector<PolyMesh::FaceHandle> restored;
		for (size_t i = 0; i < n; i += 2)
		{
			auto fh = m_mesh.add_face({m_vh, neighbors[i], neighbors[i + 1], neighbors[(i + 2) % n]});
			if (!fh.is_valid())
			{
				restoreFailed = true;
				break;
			}
			restored.push_back(fh);
		}

		if (restoreFailed)
		{
			// We cannot cleanly recover — the mesh is now in a torn state
			// (some of m_vh's original faces are back, some aren't).
			assert(false && "VertexRotation rollback failed to restore original topology");
		}

		return {{}, {}, {}};
	}

	std::unordered_set<OpenMesh::VertexHandle, MeshHandleHasher> affectedVhs{m_vh};
	std::unordered_set<OpenMesh::EdgeHandle, MeshHandleHasher> affectedEhs;
	std::unordered_set<OpenMesh::HalfedgeHandle, MeshHandleHasher> affectedDhs;

	// gather all affected vertices
	for (auto voh_it = m_mesh.voh_iter(m_vh); voh_it.is_valid(); ++voh_it)
	{
		auto heh0 = m_mesh.next_halfedge_handle(*voh_it);
		auto heh1 = m_mesh.next_halfedge_handle(heh0);
		auto vh0 = m_mesh.to_vertex_handle(heh0);
		auto vh1 = m_mesh.to_vertex_handle(heh1);

		affectedVhs.insert(vh0);
		affectedVhs.insert(vh1);
	}

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

static auto CalculateProfitability(const PolyMesh &mesh, OpenMesh::VertexHandle vh) -> float
{
	float profit = 0.0f;
	const auto &p0 = mesh.point(vh);

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