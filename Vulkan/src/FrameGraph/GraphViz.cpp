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
	std::vector<std::string> cluster_keys;
};

// Connects one to many
struct Edge
{
	std::string start;
	std::vector<std::string> ends;
	std::string color = "yellowgreen";
};

struct Graph
{
	std::vector<Node> nodes;
	std::vector<Edge> edges;
};

std::ostream &operator <<(std::ostream &os, const Graph &graph)
{
	os << "digraph G {\n";
	os << "rankdir=LR;\n";
	os << "node [style=filled shape=record]\n";

	// Nodes
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

static std::string get_key(const ResourceNode &node)
{
	auto key = "Resource" + std::to_string(node.getResource()) + "_" + std::to_string(node.getVersion());
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

	for (const auto &pass : fg.renderpass_nodes)
	{
		auto key = get_key(pass);
		const char *color = (pass.getRefCount() > 0 && !pass.hasSideEffect()) ? "orangered" : "orangered4";

		std::ostringstream title;
		title << "{" << pass.getName() << "} | {Refs: " << pass.getRefCount() << "<BR/>Index: " << pass.getId() << "}";

		std::string cluster_name;
		// if big cluster, then name it
		if (pass.getCreates().size() > 1)
		{
			cluster_name = pass.getName();
			cluster_name = std::regex_replace(cluster_name, std::regex("\\Pass"), "");
		}

		auto &new_node = graph.nodes.emplace_back(Node{key, title.str(), cluster_name});

		// Add edge where this pass writes
		auto &edge = graph.edges.emplace_back(Edge{key, {}, color});
		for (const auto &write : pass.getWrites())
		{
			ResourceNode &resource_node = fg.resource_nodes[write.resource];
			edge.ends.emplace_back(get_key(resource_node));
		}

		// Cluster producer with its resources (for example GBuffer with all its outputs)
		for (const auto &create : pass.getCreates())
		{
			ResourceNode &resource_node = fg.resource_nodes[create];
			new_node.cluster_keys.emplace_back(get_key(resource_node));
		}
	}

	for (const auto &resource : fg.resource_nodes)
	{
		auto key = get_key(resource);
		const char *color = "skyblue";

		auto &entry = fg.getResourceEntry(resource.getResource());


		std::ostringstream title;
		title << "{" << resource.getName() << " (" << resource.getVersion() << ")} | {Refs: " << resource.getRefCount() << "<BR/>Index: " << entry.resource_id << "}";

		graph.nodes.emplace_back(Node{key, title.str(), "", color, false});

		// Add edge where this resource will be read
		auto &edge = graph.edges.emplace_back(Edge{key, {}});
		for (const auto &pass : fg.renderpass_nodes)
		{
			if (pass.isReading(resource.getId()))
			{
				edge.ends.emplace_back(get_key(pass));
			}
		}
	}

	os << graph;
}
