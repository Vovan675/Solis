#pragma once
#include "FrameGraph.h"
#include "string"
#include "fstream"

class GraphViz
{
public:
	void show(std::string filename, FrameGraph &fg)
	{
        std::ofstream file(filename);
        createGraph(file, fg);
        file.close();

        std::string output_file = filename + ".png";
        std::string create_png_command = "dot -Tpng " + filename + " -o " + output_file;
        system(create_png_command.c_str());

        std::string open_png_command = "start " + output_file;
        system(open_png_command.c_str());
	}

    void createGraph(std::ostream &os, FrameGraph &fg);
};