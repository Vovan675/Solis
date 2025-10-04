#include "pch.h"
#include "EditorApplication.h"

int main(int argc, char *argv[])
{
	EditorApplication app(argc, argv);
	app.run();
	return 0;
}

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 615;}
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }