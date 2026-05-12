#pragma once

#include "MeshRenderer/SpecialVertexDatas.hpp"

class Renderable3D : public Geometry<glm::vec3, Vertex3DUniformedAttribs>
{
public:
	Renderable3D(Shader &shader, GLenum type, const glm::vec4 &color)
		: m_shader(shader), m_type(type), m_color(color)
	{
	}

	auto SetType(GLenum type) -> void { m_type = type; }

	auto Draw(const RenderState &renderState) const -> void
	{
		m_shader.Use();
		m_shader.SetUniform(renderState.MVP, "MVP");
		m_shader.SetUniform(m_color, "color");
		Geometry::Draw(m_type);
	}

private:
	Shader &m_shader;
	GLenum m_type;
	glm::vec4 m_color;
};