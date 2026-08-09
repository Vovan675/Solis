#pragma once
#include "RendererBase.h"
#include "Rendering/Mesh.h"
#include "Utils/Camera.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphRHIResources.h"
#include "DDGIRenderer.h"

class DefferedCompositeRenderer: public RendererBase
{
public:
	DefferedCompositeRenderer();
	virtual ~DefferedCompositeRenderer();

	void addPasses(FrameGraph &fg);

private:
};

