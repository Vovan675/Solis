#pragma once
#include <sl.h>

class StreamlineWrapper
{
public:
	static bool init();
	static void shutdown();
	static bool isInitialized() { return initialized; }
	static bool isFeatureSupported(sl::Feature feature);

	static sl::FrameToken *setFrameConstants(const sl::ViewportHandle &viewport, bool reset_history);

private:
	static bool initialized;
};
