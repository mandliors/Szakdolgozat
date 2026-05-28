#include "ApproximatingMesh.hpp"

int32_t ApproximatingMesh::s_surfaceResolution = 5;

ApproximatingMesh::ApproximatingMesh(const PolyMesh& topologyMesh, const Shader& faceShader, const Shader& edgeShader, const Shader& pointShader, const glm::vec4& faceColor, const glm::vec4& edgeColor, const glm::vec4& pointColor)
	: m_cps(topologyMesh), m_sfc(topologyMesh)
{
	m_cpsRenderer = std::make_unique<MeshRenderer>(m_cps, faceShader, edgeShader, pointShader, faceColor, faceColor, pointColor);
	m_cpsRenderer->SetDrawMode(false, false, true);

	m_sfcRenderer = std::make_unique<MeshRenderer>(m_sfc, faceShader, edgeShader, pointShader, faceColor, edgeColor, pointColor);
	m_sfcRenderer->SetDrawMode(true, false, false);
}

auto ApproximatingMesh::UpdateGPU() -> void
{
	m_sfcRenderer->UpdateGPU();
	m_cpsRenderer->UpdateGPU();
}
auto ApproximatingMesh::Draw(const RenderState& renderState) const -> void
{
	m_sfcRenderer->Draw(renderState);
	m_cpsRenderer->Draw(renderState);
}
auto ApproximatingMesh::Reset() -> void
{
	m_sfc = m_cps;
	if (m_subdivider.attach(m_sfc))
	{
		m_subdivider(s_surfaceResolution);
		m_subdivider.detach();
	}
}

auto ApproximatingMesh::ShowCoarseMesh(bool show) -> void
{
	m_cpsRenderer->SetDrawMode(false, show, true);
}

auto ApproximatingMesh::TopologyMesh() -> PolyMesh&
{
	return m_cps;
}
auto ApproximatingMesh::RenderMesh() -> MeshRenderer&
{
	return *m_sfcRenderer;
}

auto ApproximatingMesh::SetSurfaceResolution(int32_t resolution) -> void
{
	s_surfaceResolution = resolution;
}
