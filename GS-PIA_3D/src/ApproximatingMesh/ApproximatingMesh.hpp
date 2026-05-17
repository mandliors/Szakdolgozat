#pragma once

#include "MeshRenderer/MeshRenderer.hpp"

#include "OpenMesh/Core/IO/MeshIO.hh"
#include "OpenMesh/Core/Mesh/PolyMesh_ArrayKernelT.hh"
#include "OpenMesh/Tools/Subdivider/Uniform/CatmullClarkT.hh"

#include <memory>

using PolyMesh = OpenMesh::PolyMesh_ArrayKernelT<>;
using Subdivider = OpenMesh::Subdivider::Uniform::CatmullClarkT<PolyMesh>;

class ApproximatingMesh
{
public:
	ApproximatingMesh(const PolyMesh &topologyMesh, const Shader &faceShader, const Shader &edgeShader, const Shader &pointShader, const glm::vec4 &faceColor, const glm::vec4 &edgeColor, const glm::vec4 &pointColor);

	auto UpdateGPU() -> void;
	auto Draw(const RenderState &renderState) const -> void;
	auto Reset() -> void;

	auto TopologyMesh() -> PolyMesh &;
	auto RenderMesh() -> MeshRenderer &;

public:
	static auto SetSurfaceResolution(int32_t resolution) -> void;

private:
	static int32_t s_surfaceResolution;

private:
	PolyMesh m_cps;
	PolyMesh m_sfc;

	std::unique_ptr<MeshRenderer> m_cpsRenderer;
	std::unique_ptr<MeshRenderer> m_sfcRenderer;

	Subdivider m_subdivider;
};