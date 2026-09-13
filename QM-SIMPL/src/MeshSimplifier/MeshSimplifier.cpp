#include "MeshSimplifier.hpp"

#include "Operations/Optimizing/VertexRotation/VertexRotation.hpp"
#include "Operations/Optimizing/EdgeRotation/EdgeRotation.hpp"
#include "Operations/Coarsening/EdgeCollapse/EdgeCollapse.hpp"
#include "Operations/Coarsening/DiagonalCollapse/DiagonalCollapse.hpp"

static auto ProjectToTriangle(const OpenMesh::Vec3f &p, const OpenMesh::Vec3f &q1, const OpenMesh::Vec3f &q2, const OpenMesh::Vec3f &q3) -> OpenMesh::Vec3f;
static auto ProjectToQuad(const OpenMesh::Vec3f &p, const OpenMesh::Vec3f &q1, const OpenMesh::Vec3f &q2, const OpenMesh::Vec3f &q3, const OpenMesh::Vec3f &q4) -> OpenMesh::Vec3f;
static auto ProjectToFace(const PolyMesh &mesh, OpenMesh::FaceHandle fh, const OpenMesh::Vec3f &p) -> OpenMesh::Vec3f;
static auto DistanceSqrToFace(const PolyMesh &mesh, OpenMesh::FaceHandle fh, const OpenMesh::Vec3f &p) -> float;

MeshSimplifier::MeshSimplifier(const PolyMesh &originalMesh, PolyMesh &topologyMesh)
	: m_mesh(topologyMesh), m_originalMesh(originalMesh), m_operationHeap(topologyMesh)
{
	m_mesh.request_vertex_status();
	m_mesh.request_edge_status();
	m_mesh.request_halfedge_status();
	m_mesh.request_face_status();

	BuildCloud();
	m_centroidTree = std::make_unique<KDTree>(3, *m_faceCentroidCloud, nanoflann::KDTreeSingleIndexAdaptorParams(10));
	m_centroidTree->buildIndex();

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
			// throw away all invalid operations
			if (!op->IsValid())
			{
				op = m_operationHeap.Pop();
				continue;
			}

			bool coarsening = op->GetType() == OperationType::COARSENING;

			// throw away all optimizing operations with negative profitability
			while (!coarsening && op->GetProfitability() < std::numeric_limits<float>::epsilon())
			{
				op = m_operationHeap.Pop();
				if (!op)
					return;

				coarsening = op->GetType() == OperationType::COARSENING;
			}

			op->Print();

			auto [vhs, ehs, dhs] = op->Execute();
			if (coarsening)
			{
				auto vhsCloned = vhs;

				// perform local (tangent-space) smoothing on the affected vertices
				for (const auto vh : vhsCloned)
				{
					auto [vhs_, ehs_, dhs_] = ProjectVertex(vh);
					vhs.insert(vhs_.begin(), vhs_.end());
					ehs.insert(ehs_.begin(), ehs_.end());
					dhs.insert(dhs_.begin(), dhs_.end());
				}
			}

			// add new operations for the affected vertices, edges and diagonals
			for (const auto vh : vhs)
				AddVertexOperations(vh, ++m_mesh.property(m_vertexTimestamp, vh));
			for (const auto eh : ehs)
				AddEdgeOperations(eh, ++m_mesh.property(m_edgeTimestamp, eh));
			for (const auto heh : dhs)
				AddDiagonalOperations(heh, ++m_mesh.property(m_diagonalTimestamp, heh));

			if (coarsening)
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

auto MeshSimplifier::BuildCloud() -> void
{
	m_faceCentroidCloud = std::make_unique<FaceCentroidCloud>();
	for (auto fh : m_originalMesh.faces())
	{
		auto c = m_originalMesh.calc_face_centroid(fh);
		m_faceCentroidCloud->faces.emplace_back(c, fh);
	}
}
auto MeshSimplifier::ProjectVertex(OpenMesh::VertexHandle vh) -> std::tuple<
	std::unordered_set<PolyMesh::VertexHandle, MeshHandleHasher>,
	std::unordered_set<PolyMesh::EdgeHandle, MeshHandleHasher>,
	std::unordered_set<PolyMesh::HalfedgeHandle, MeshHandleHasher>>
{
	auto &p = m_mesh.point(vh);

	OpenMesh::FaceHandle closestFace = ClosestFace(p);
	if (closestFace.is_valid())
		p = ProjectToFace(m_originalMesh, closestFace, p);

	std::unordered_set<OpenMesh::VertexHandle, MeshHandleHasher> affectedVhs{vh};
	std::unordered_set<OpenMesh::EdgeHandle, MeshHandleHasher> affectedEhs;
	std::unordered_set<OpenMesh::HalfedgeHandle, MeshHandleHasher> affectedDhs;

	// gather all affected vertices
	for (auto voh_it = m_mesh.voh_iter(vh); voh_it.is_valid(); ++voh_it)
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
auto MeshSimplifier::ClosestFace(const OpenMesh::Vec3f &p, size_t candidateCount) const -> OpenMesh::FaceHandle
{
	double query[3] = {p[0], p[1], p[2]};

	// fetch n nearest centroids
	std::vector<size_t> idxs(candidateCount);
	std::vector<double> dists(candidateCount);
	nanoflann::KNNResultSet<double> resultSet(candidateCount);

	resultSet.init(idxs.data(), dists.data());
	m_centroidTree->findNeighbors(resultSet, query, nanoflann::SearchParameters());

	// among candidates, find the one with minimum exact point-to-quad distance
	OpenMesh::FaceHandle best;
	double bestDist = std::numeric_limits<double>::max();
	for (size_t i = 0; i < candidateCount; i++)
	{
		double d = DistanceSqrToFace(m_originalMesh, m_faceCentroidCloud->faces[idxs[i]].second, p);
		if (d < bestDist)
		{
			bestDist = d;
			best = m_faceCentroidCloud->faces[idxs[i]].second;
		}
	}

	return best;
}

static auto ProjectToTriangle(const OpenMesh::Vec3f &p, const OpenMesh::Vec3f &q1, const OpenMesh::Vec3f &q2, const OpenMesh::Vec3f &q3) -> OpenMesh::Vec3f
{
	// Peter Salvi's implementation of the algorithm for projecting a point onto a triangle (with some minor modifications)
	// As in Schneider, Eberly: Geometric Tools for Computer Graphics, Morgan Kaufmann, 2003.
	// Section 10.3.2, pp. 376-382 (with Peter Salvi's corrections)

	const auto &P = p, &B = q1;
	const auto E0 = q2 - q1, E1 = q3 - q1, D = B - P;
	const auto a = E0 | E0, b = E0 | E1, c = E1 | E1, d = E0 | D, e = E1 | D;
	auto det = a * c - b * b, s = b * e - c * d, t = b * d - a * e;

	if (s + t <= det)
	{
		if (s < 0)
		{
			if (t < 0)
			{
				// Region 4
				if (e < 0)
				{
					s = 0.0;
					t = (-e >= c ? 1.0 : -e / c);
				}
				else if (d < 0)
				{
					t = 0.0;
					s = (-d >= a ? 1.0 : -d / a);
				}
				else
				{
					s = 0.0;
					t = 0.0;
				}
			}
			else
			{
				// Region 3
				s = 0.0;
				t = (e >= 0.0 ? 0.0 : (-e >= c ? 1.0 : -e / c));
			}
		}
		else if (t < 0)
		{
			// Region 5
			t = 0.0;
			s = (d >= 0.0 ? 0.0 : (-d >= a ? 1.0 : -d / a));
		}
		else
		{
			// Region 0
			double invDet = 1.0 / det;
			s *= invDet;
			t *= invDet;
		}
	}
	else
	{
		if (s < 0)
		{
			// Region 2
			double tmp0 = b + d, tmp1 = c + e;
			if (tmp1 > tmp0)
			{
				double numer = tmp1 - tmp0;
				double denom = a - 2 * b + c;
				s = (numer >= denom ? 1.0 : numer / denom);
				t = 1.0 - s;
			}
			else
			{
				s = 0.0;
				t = (tmp1 <= 0.0 ? 1.0 : (e >= 0.0 ? 0.0 : -e / c));
			}
		}
		else if (t < 0)
		{
			// Region 6
			double tmp0 = b + e, tmp1 = a + d;
			if (tmp1 > tmp0)
			{
				double numer = tmp1 - tmp0;
				double denom = c - 2 * b + a;
				t = (numer >= denom ? 1.0 : numer / denom);
				s = 1.0 - t;
			}
			else
			{
				t = 0.0;
				s = (tmp1 <= 0.0 ? 1.0 : (d >= 0.0 ? 0.0 : -d / a));
			}
		}
		else
		{
			// Region 1
			double numer = c + e - b - d;
			if (numer <= 0)
			{
				s = 0;
			}
			else
			{
				double denom = a - 2 * b + c;
				s = (numer >= denom ? 1.0 : numer / denom);
			}
			t = 1.0 - s;
		}
	}
	return B + E0 * s + E1 * t;
}
static auto ProjectToQuad(const OpenMesh::Vec3f &p, const OpenMesh::Vec3f &q1, const OpenMesh::Vec3f &q2, const OpenMesh::Vec3f &q3, const OpenMesh::Vec3f &q4) -> OpenMesh::Vec3f
{
	const auto p1 = ProjectToTriangle(p, q1, q2, q3);
	const auto p2 = ProjectToTriangle(p, q1, q3, q4);
	const auto d1 = (p1 - p).sqrnorm();
	const auto d2 = (p2 - p).sqrnorm();

	return (d1 < d2) ? p1 : p2;
}
static auto ProjectToFace(const PolyMesh &mesh, OpenMesh::FaceHandle fh, const OpenMesh::Vec3f &p) -> OpenMesh::Vec3f
{
	auto vtxCount = std::distance(mesh.fv_range(fh).begin(), mesh.fv_range(fh).end());
	if (vtxCount != 4)
		return OpenMesh::Vec3f{0.0f, 0.0f, 0.0f};

	auto fv_it = mesh.cfv_iter(fh);
	auto q1 = mesh.point(*fv_it);
	auto q2 = mesh.point(*(++fv_it));
	auto q3 = mesh.point(*(++fv_it));
	auto q4 = mesh.point(*(++fv_it));

	return ProjectToQuad(p, q1, q2, q3, q4);
}
static auto DistanceSqrToFace(const PolyMesh &mesh, OpenMesh::FaceHandle fh, const OpenMesh::Vec3f &p) -> float
{
	auto vtxCount = std::distance(mesh.fv_range(fh).begin(), mesh.fv_range(fh).end());
	if (vtxCount != 4)
		return -1.0f;

	auto fv_it = mesh.cfv_iter(fh);
	auto q1 = mesh.point(*fv_it);
	auto q2 = mesh.point(*(++fv_it));
	auto q3 = mesh.point(*(++fv_it));
	auto q4 = mesh.point(*(++fv_it));

	auto p1 = ProjectToTriangle(p, q1, q2, q3);
	auto p2 = ProjectToTriangle(p, q1, q3, q4);
	auto d1 = (p1 - p).sqrnorm();
	auto d2 = (p2 - p).sqrnorm();

	return (d1 < d2) ? d1 : d2;
}