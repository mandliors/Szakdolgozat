#pragma once

#include "Geometry/Geometry.hpp"
#include "MeshRenderer/SpecialVertexDatas.hpp"

class Renderable2D : public Geometry<glm::vec2, Vertex2DUniformedAttribs>
{
public:
	Renderable2D(Shader &shader, GLenum type, const glm::vec4 &color)
		: m_shader(shader), m_type(type), m_color(color)
	{
	}

	auto SetType(GLenum type) -> void { m_type = type; }

	auto Draw() const -> void
	{
		m_shader.Use();
		m_shader.SetUniform(m_color, "color");
		m_shader.SetUniform(m_type == GL_POINTS, "point");
		Geometry::Draw(m_type);
	}

private:
	Shader &m_shader;
	GLenum m_type;
	glm::vec4 m_color;
};