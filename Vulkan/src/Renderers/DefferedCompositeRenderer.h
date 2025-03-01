#pragma once
#include "RendererBase.h"
#include "Mesh.h"
#include "Camera.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphRHIResources.h"

class DefferedCompositeRenderer: public RendererBase
{
public:
	DefferedCompositeRenderer();
	virtual ~DefferedCompositeRenderer();

	void addPasses(FrameGraph &fg);

	void renderImgui();
private:
};

