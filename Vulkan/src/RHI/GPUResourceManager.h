#pragma once
class GPUResource
{
public:
	virtual ~GPUResource() = default;

	virtual void destroy() = 0; 

protected:
};

class GPUResourceManager
{
public:
	void registerResource(std::shared_ptr<GPUResource> resource)
	{
		resources.push_back(resource);
	}

	// Remove expired resources.
	void removeUnusedResources() {
		for (auto it = resources.begin(); it != resources.end();) {
			if (it->expired()) {
				it = resources.erase(it); // Remove expired resources
			} else {
				++it;
			}
		}
	}

	// Cleanup all GPU resources.
	void cleanup() {
		for (auto& entry : resources) {
			if (auto resource = entry.lock()) {
				resource->destroy(); // Safely destroy resource
			}
		}
		resources.clear();
	}

	std::vector<std::weak_ptr<GPUResource>> resources;
};
