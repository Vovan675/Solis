#include "pch.h"
#include "GraphViz.h"
#include <algorithm>
#include <regex>

struct Node
{
	std::string key;
	std::string label;
	std::string cluster_name;
	std::string color = "orange";
	bool is_rounded = true;
	eastl::vector<std::string> cluster_keys;
};

// Connects one to many
struct Edge
{
	std::string start;
	eastl::vector<std::string> ends;
	std::string color = "yellowgreen";
};

struct Graph
{
	eastl::vector<Node> nodes;
	eastl::vector<Edge> edges;
};

std::ostream &operator <<(std::ostream &os, const Graph &graph)
{
	os << "digraph G {\n";
	os << "rankdir=LR;\n";
	os << "node [style=filled shape=record]\n";

	// Nodes
	os << "// Nodes\n";
	for (const Node &node : graph.nodes)
	{
		os << node.key << "[label=<" << node.label << ">" << " fillcolor=" << node.color << 
			(node.is_rounded ? " style=<filled,rounded>" : "") << 
			"]\n";

		if (!node.cluster_keys.empty())
		{
			os << "subgraph cluster_" << node.key << " {\n" << node.key << "\n";

			if (!node.cluster_name.empty())
				os << "label = " << "<" << node.cluster_name << ">\n";

			for (const auto &key : node.cluster_keys)
			{
				os << key << "\n";
			}
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
		os << "} [" << "color=" << edge.color << "]\n";
	}

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

	auto addTextureNode = [&graph](FrameGraphTexture *texture)
	{
		auto node_key = get_key(texture);
		const char *color = "skyblue";

		std::ostringstream title;
		title << "{" << texture->name.c_str() << " (" << texture->version << ")} | {Refs: " << texture->ref_count << "<BR/>Index: " << texture->resource_id << "<BR/>" << texture->toString().c_str() << "}";

		graph.nodes.emplace_back(Node{node_key, title.str(), "", color, false});
	};

	auto addBufferNode = [&graph](FrameGraphBuffer *buffer)
	{
		auto node_key = get_key(buffer);
		const char *color = "skyblue";

		std::ostringstream title;
		title << "{" << buffer->name.c_str() << " (" << buffer->version << ")} | {Refs: " << buffer->ref_count << "<BR/>Index: " << buffer->resource_id << "<BR/>" << buffer->toString().c_str() << "}";

		graph.nodes.emplace_back(Node{node_key, title.str(), "", color, false});
	};

	for (const auto &pass : fg.renderpass_nodes)
	{
		auto pass_key = get_key(pass);
		const char *node_color = (pass.getRefCount() > 0 || pass.hasSideEffect()) ? "orange" : "orangered4";
		const char *edge_color = (pass.getRefCount() > 0 || pass.hasSideEffect()) ? "red" : "red4";

		std::ostringstream title;
		title << "{" << pass.getName().c_str() << "} | {Refs: " << pass.getRefCount() << "<BR/>Index: " << pass.getId() << "}";

		std::string cluster_name;
		// if big cluster, then name it
		if ((pass.getTextureWrites().size() + pass.getBufferWrites().size()) > 1)
		{
			cluster_name = pass.getName().c_str();
			cluster_name = std::regex_replace(cluster_name, std::regex("\\Pass"), "");
		}

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

		auto &new_node = graph.nodes.emplace_back(Node{pass_key, title.str(), cluster_name, node_color});

		// Cluster producer with its resources (for example GBuffer with all its outputs)
		for (const auto &write : pass.getTextureWrites())
		{
			FrameGraphTexture *resource_node = fg.getFrameGraphTexture(write);
			new_node.cluster_keys.emplace_back(get_key(resource_node));
		}

		for (const auto &write : pass.getBufferWrites())
		{
			FrameGraphBuffer *resource_node = fg.getFrameGraphBuffer(write);
			new_node.cluster_keys.emplace_back(get_key(resource_node));
		}
	}

	/*
	for (const auto &resource : fg.all_textures)
	{
		auto resource_key = get_key(&resource);
		const char *color = "skyblue";

		//auto &entry = fg.getResourceEntry(resource);


		std::ostringstream title;
		title << "{" << resource.name.c_str() << " (" << resource.version << ")} | {Refs: " << resource.ref_count << "<BR/>Index: " << resource.resource_id << "<BR/>" << resource.toString().c_str() << "}";

		graph.nodes.emplace_back(Node{resource_key, title.str(), "", color, false});

		// Add edge where this resource will be read
		auto &edge = graph.edges.emplace_back(Edge{resource_key, {}});
		for (const auto &pass : fg.renderpass_nodes)
		{
			if (pass.isReading(resource.resource_id))
			{
				edge.ends.emplace_back(get_key(pass));
			}
		}
	}
	*/

	os << graph;
}
