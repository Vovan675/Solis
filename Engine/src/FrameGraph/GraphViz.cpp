#include "pch.h"
#include "GraphViz.h"

static const char *PASS_COLOR = "#f55938";
static const char *PASS_FILL = "#3a1d15";
static const char *PASS_TEXT = "#f0f3f6";
static const char *READ_COLOR = "#3fb9a0";
static const char *TEXT_COLOR = "#c9d1d9";
static const char *MUTED_COLOR = "#8b949e";
static const char *TEXTURE_COLOR = "#58a6ff";
static const char *TEXTURE_FILL = "#16233a";
static const char *BUFFER_COLOR = "#6e7681";
static const char *BUFFER_FILL = "#1c2128";

struct Node
{
	std::string key;
	std::string label;
	std::string style;
	eastl::vector<std::string> output_keys;
};

// Connects one to many
struct Edge
{
	std::string start;
	eastl::vector<std::string> ends;
	std::string color = READ_COLOR;
};

struct Graph
{
	eastl::vector<Node> nodes;
	eastl::vector<Edge> edges;
};

std::ostream &operator <<(std::ostream &os, const Graph &graph)
{
	os << "digraph G {\n";
	os << "rankdir=TB bgcolor=\"#0d1117\" pad=0.4 nodesep=0.25 ranksep=0.45 newrank=true fontname=\"Roboto\"\n";
	os << "node [shape=box fontname=\"Roboto\" fontsize=16 fontcolor=\"" << TEXT_COLOR << "\" penwidth=1.6]\n";
	os << "edge [fontname=\"Roboto\" fontsize=15 penwidth=1.3 arrowsize=0.6]\n";

	// Nodes
	os << "// Nodes\n";
	for (const Node &node : graph.nodes)
	{
		os << node.key << "[label=<" << node.label << "> " << node.style << "]\n";

		// Outputs of a pass in one row right under it
		if (!node.output_keys.empty())
		{
			os << "{ rank=same; ";
			for (const auto &key : node.output_keys)
				os << key << " ";
			os << "}\n";
		}
	}

	// Edges
	os << "// Edges\n";
	for (const Edge &edge : graph.edges)
	{
		os << edge.start << "->{ ";
		for (const std::string &end : edge.ends)
		{
			os << end << " ";
		}
		os << "} [" << "color=\"" << edge.color << "\"]\n";
	}

	// Legend
	os << "labelloc=t labeljust=r label=<<TABLE BORDER=\"1\" COLOR=\"#30363d\" STYLE=\"rounded\" CELLSPACING=\"8\" CELLPADDING=\"6\">";
	os << "<TR><TD COLSPAN=\"2\" BORDER=\"0\"><FONT COLOR=\"" << TEXT_COLOR << "\" POINT-SIZE=\"18\"><B>Legend</B></FONT></TD></TR>";
	os << "<TR><TD COLSPAN=\"2\" BORDER=\"1\" STYLE=\"rounded\" BGCOLOR=\"" << PASS_FILL << "\" COLOR=\"" << PASS_COLOR << "\"><FONT COLOR=\"" << PASS_TEXT << "\"><B>Pass</B></FONT></TD></TR>";
	os << "<TR><TD BORDER=\"1\" STYLE=\"rounded\" BGCOLOR=\"" << TEXTURE_FILL << "\" COLOR=\"" << TEXTURE_COLOR << "\"><FONT COLOR=\"" << TEXT_COLOR << "\">Texture</FONT></TD>";
	os << "<TD BORDER=\"1\" STYLE=\"rounded\" BGCOLOR=\"" << BUFFER_FILL << "\" COLOR=\"" << BUFFER_COLOR << "\"><FONT COLOR=\"" << TEXT_COLOR << "\">Buffer</FONT></TD></TR>";
	os << "<TR><TD BORDER=\"1\" STYLE=\"rounded,dashed\" BGCOLOR=\"" << BUFFER_FILL << "\" COLOR=\"" << BUFFER_COLOR << "\"><FONT COLOR=\"" << TEXT_COLOR << "\">Transient</FONT></TD>";
	os << "<TD BORDER=\"1\" STYLE=\"rounded\" BGCOLOR=\"" << BUFFER_FILL << "\" COLOR=\"" << BUFFER_COLOR << "\"><FONT COLOR=\"" << TEXT_COLOR << "\">Imported</FONT></TD></TR>";
	os << "<TR><TD BORDER=\"0\"><FONT COLOR=\"" << PASS_COLOR << "\">write</FONT></TD><TD BORDER=\"0\"><FONT COLOR=\"" << READ_COLOR << "\">read</FONT></TD></TR>";
	os << "</TABLE>>\n";

	os << "}\n";
	return os;
}

static std::string get_key(const FrameGraphTexture *texture)
{
	auto key = "Resource_Texture_" + std::to_string(texture->resource_id) + "_" + std::to_string(texture->version);
	return key;
}

static std::string get_key(const FrameGraphBuffer *buffer)
{
	auto key = "Resource_Buffer_" + std::to_string(buffer->resource_id) + "_" + std::to_string(buffer->version);
	return key;
}

static std::string get_key(const RenderPassNode &node)
{
	auto key = "Pass" + std::to_string(node.getId());
	return key;
}

void GraphViz::createGraph(std::ostream &os, FrameGraph &fg)
{
	Graph graph;

	std::string declarations;
	std::string dependencies;

	auto addResourceNode = [&graph](const std::string &node_key, const eastl::string &name, const eastl::string &details, bool is_texture, bool is_transient)
	{
		std::ostringstream title;
		title << name.c_str() << "<BR/><FONT POINT-SIZE=\"13\" COLOR=\"" << MUTED_COLOR << "\">" << details.c_str() << "</FONT>";

		std::ostringstream style;
		style << "style=\"rounded,filled" << (is_transient ? ",dashed" : "") << "\" "
			<< "fillcolor=\"" << (is_texture ? TEXTURE_FILL : BUFFER_FILL)
			<< "\" color=\"" << (is_texture ? TEXTURE_COLOR : BUFFER_COLOR) << "\"";

		graph.nodes.emplace_back(Node{node_key, title.str(), style.str()});
	};

	auto addTextureNode = [&](FrameGraphTexture *texture)
	{
		addResourceNode(get_key(texture), texture->name, texture->toString(), true, texture->is_transient);
	};

	auto addBufferNode = [&](FrameGraphBuffer *buffer)
	{
		addResourceNode(get_key(buffer), buffer->name, buffer->toString(), false, buffer->is_transient);
	};

	for (const auto &pass : fg.renderpass_nodes)
	{
		auto pass_key = get_key(pass);
		bool is_culled = pass.getRefCount() == 0 && !pass.hasSideEffect();
		const char *edge_color = is_culled ? MUTED_COLOR : PASS_COLOR;

		std::ostringstream style;
		style << "style=\"rounded,filled\" fontsize=19 fontcolor=\"" << PASS_TEXT << "\" fillcolor=\"" << (is_culled ? BUFFER_FILL : PASS_FILL) << "\" color=\"" << edge_color << "\"";

		std::ostringstream title;
		title << "<B>" << pass.getName().c_str() << "</B>";

		// Add edges

		// Textures
		{
			for (const auto &read : pass.getTextureReads())
			{
				FrameGraphTexture *resource = fg.getFrameGraphTexture(read);
				addTextureNode(resource);
				auto &edge = graph.edges.emplace_back(Edge{get_key(resource), {pass_key}});
			}

			auto &edge = graph.edges.emplace_back(Edge{pass_key, {}, edge_color});
			for (const auto &write : pass.getTextureWrites())
			{
				FrameGraphTexture *resource = fg.getFrameGraphTexture(write);
				// If someone other than creator texture_writes to resource, then resource version is up
				if (!pass.isCreating(write)) resource->version++;
				addTextureNode(resource);
				edge.ends.emplace_back(get_key(resource));
			}
		}

		// Buffers
		{
			for (const auto &read : pass.getBufferReads())
			{
				FrameGraphBuffer *resource = fg.getFrameGraphBuffer(read);
				addBufferNode(resource);
				auto &edge = graph.edges.emplace_back(Edge{get_key(resource), {pass_key}});
			}

			auto &edge = graph.edges.emplace_back(Edge{pass_key, {}, edge_color});
			for (const auto &write : pass.getBufferWrites())
			{
				FrameGraphBuffer *resource = fg.getFrameGraphBuffer(write);
				// If someone other than creator texture_writes to resource, then resource version is up
				if (!pass.isCreating(write)) resource->version++;
				addBufferNode(resource);
				edge.ends.emplace_back(get_key(resource));
			}
		}

		auto &new_node = graph.nodes.emplace_back(Node{pass_key, title.str(), style.str()});

		// Producer with its resources in one row (for example GBuffer with all its outputs)
		for (const auto &write : pass.getTextureWrites())
		{
			FrameGraphTexture *resource_node = fg.getFrameGraphTexture(write);
			new_node.output_keys.emplace_back(get_key(resource_node));
		}

		for (const auto &write : pass.getBufferWrites())
		{
			FrameGraphBuffer *resource_node = fg.getFrameGraphBuffer(write);
			new_node.output_keys.emplace_back(get_key(resource_node));
		}
	}

	os << graph;
}
