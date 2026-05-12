#pragma once

#include "OpenMesh/Core/IO/MeshIO.hh"
#include "OpenMesh/Core/Mesh/PolyMesh_ArrayKernelT.hh"

#include <unordered_set>

enum class OperationType { OPTIMIZING = 0, COARSENING };

using PolyMesh = OpenMesh::PolyMesh_ArrayKernelT<>;

struct MeshHandleHasher
{
	template <typename Handle>
	size_t operator()(const Handle& h) const {
		return static_cast<size_t>(h.idx());
	}
};

class Operation
{
public:
	Operation(float initialProfitability, int32_t timestamp) : m_profitability(initialProfitability), m_timestamp(timestamp) {}
	virtual ~Operation() = default;

	auto GetProfitability() const -> float { return m_profitability; }
	auto GetTimestamp() const -> int32_t { return m_timestamp; }
	virtual auto GetType() const -> OperationType = 0;

	virtual auto IsValid() const -> bool = 0;
	virtual auto Execute() -> std::tuple<
		std::unordered_set<PolyMesh::VertexHandle, MeshHandleHasher>,
		std::unordered_set<PolyMesh::EdgeHandle, MeshHandleHasher>,
		std::unordered_set<PolyMesh::HalfedgeHandle, MeshHandleHasher>> = 0;

	auto operator>(const Operation& other) const -> bool
	{
		if (this->GetType() != other.GetType())
			return this->GetType() == OperationType::OPTIMIZING;

		return this->GetTimestamp() > other.GetTimestamp();
	}

	virtual auto Print() const -> void = 0;

protected:
	float m_profitability;
	int32_t m_timestamp;
};