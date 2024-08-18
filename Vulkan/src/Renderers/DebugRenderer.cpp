#include "pch.h"
#include "DebugRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"

DebugRenderer::DebugRenderer()
{
	vertex_shader = Shader::create("shaders/quad.vert", Shader::VERTEX_SHADER);
	fragment_shader = Shader::create("shaders/debug_quad.frag", Shader::FRAGMENT_SHADER);

	vertex_shader_lines = Shader::create("shaders/debug_lines.vert", Shader::VERTEX_SHADER);
	fragment_shader_lines = Shader::create("shaders/debug_lines.frag", Shader::FRAGMENT_SHADER);

	uint32_t indices[256];
	for (size_t i = 0; i < 256; i++)
	{
		indices[i] = i;
	}

	BufferDescription desc;
	desc.size = sizeof(uint32_t) * 256;
	desc.useStagingBuffer = true;
	desc.bufferUsageFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

	lines_index_buffer = std::make_shared<Buffer>(desc);
	lines_index_buffer->fill(indices);


	vertices = new Engine::Vertex[256];
	
	desc.size = sizeof(Engine::Vertex) * 256;
	desc.useStagingBuffer = false;
	desc.bufferUsageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	lines_vertex_buffer = std::make_shared<Buffer>(desc);
	lines_vertex_buffer->map((void **)&vertices);
}

DebugRenderer::~DebugRenderer()
{
}

void DebugRenderer::fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index)
{
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(vertex_shader);
	p->setFragmentShader(fragment_shader);

	p->setRenderTargets(VkWrapper::current_render_targets);
	p->setUseVertices(false);
	p->setUseBlending(false);

	p->flush();
	p->bind(command_buffer);

	// Uniforms
	Renderer::setShadersUniformBuffer(vertex_shader, fragment_shader, 0, &ubo, sizeof(PresentUBO), image_index);
	Renderer::bindShadersDescriptorSets(vertex_shader, fragment_shader, command_buffer, p->getPipelineLayout(), image_index);

	// Render quad
	vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

	p->unbind(command_buffer);
}

void DebugRenderer::renderLines(CommandBuffer &command_buffer, uint32_t image_index)
{
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(vertex_shader_lines);
	p->setFragmentShader(fragment_shader_lines);

	p->setRenderTargets(VkWrapper::current_render_targets);
	p->setUseBlending(false);
	p->setPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	p->setDepthTest(false);
	p->setCullMode(VK_CULL_MODE_NONE);

	p->flush();
	p->bind(command_buffer);

	// Uniforms
	Renderer::bindShadersDescriptorSets(vertex_shader_lines, fragment_shader_lines, command_buffer, p->getPipelineLayout(), image_index);

	VkBuffer vertexBuffers[] = {lines_vertex_buffer->bufferHandle};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(command_buffer.get_buffer(), lines_index_buffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(command_buffer.get_buffer(), lines_index_count, 1, 0, 0, 0);

	p->unbind(command_buffer);

	lines_index_count = 0;
}

std::vector<glm::vec3> DebugRenderer::addCirlce(glm::vec3 center, glm::vec3 normal, float radius, int segments)
{
	std::vector<glm::vec3> circle_points;
	glm::vec3 tangent1, tangent2;

	// Create tangents based on the normal
	if (fabs(normal.x) > fabs(normal.y))
	{
		tangent1 = glm::normalize(glm::vec3(normal.z, 0, -normal.x));
	} else
	{
		tangent1 = glm::normalize(glm::vec3(0, -normal.z, normal.y));
	}
	tangent2 = glm::cross(normal, tangent1);

	for (int i = 0; i < segments; ++i)
	{
		float theta = glm::two_pi<float>() * float(i) / float(segments);
		glm::vec3 circle_point = center + radius * (cos(theta) * tangent1 + sin(theta) * tangent2);
		circle_points.push_back(circle_point);
	}

	// Add lines to draw the circle
	for (int i = 0; i < segments; ++i)
	{
		addLine(circle_points[i], circle_points[(i + 1) % segments]);
	}

	return circle_points;
}

void DebugRenderer::addArrow(glm::vec3 p0, glm::vec3 p1, float arrow_size)
{
	// Add the main line from p0 to p1
	addLine(p0, p1);

	// Calculate the direction from p0 to p1
	glm::vec3 direction = glm::normalize(p1 - p0);

	// Calculate circle at the base of the arrow
	float circle_radius = arrow_size * 0.5f; // Adjust circle radius as needed
	std::vector<glm::vec3> circle_points = addCirlce(p1 - direction * arrow_size, direction, circle_radius, 12);

	// Add lines from the circle to the arrow tip
	for (const auto &point : circle_points)
	{
		addLine(point, p1);
	}

	// Calculate perpendicular vectors to create the arrowhead
	glm::vec3 perpendicular1 = glm::normalize(glm::cross(direction, glm::vec3(0, 1, 0)));
	glm::vec3 perpendicular2 = glm::normalize(glm::cross(direction, perpendicular1));

	glm::vec3 arrowhead_point1 = p1 - direction * arrow_size + perpendicular1 * (arrow_size / 2.0f);
	glm::vec3 arrowhead_point2 = p1 - direction * arrow_size - perpendicular1 * (arrow_size / 2.0f);
	glm::vec3 arrowhead_point3 = p1 - direction * arrow_size + perpendicular2 * (arrow_size / 2.0f);
	glm::vec3 arrowhead_point4 = p1 - direction * arrow_size - perpendicular2 * (arrow_size / 2.0f);

	// Add the arrowhead lines
	addLine(p1, arrowhead_point1);
	addLine(p1, arrowhead_point2);
	addLine(p1, arrowhead_point3);
	addLine(p1, arrowhead_point4);
}
