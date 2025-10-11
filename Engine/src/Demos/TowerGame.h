#pragma once
#include "RHI/DynamicRHI.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"
#include "Rendering/SceneRenderer.h"

struct Block {
    Entity entity;
    glm::vec3 size;
    glm::vec3 position;
    glm::vec4 color;

    Block(const glm::vec3& s, const glm::vec3& p, const glm::vec4& c) 
        : size(s), position(p), color(c) {}
};

class Layer {
public:
    std::shared_ptr<Block> movingBlock;
    std::shared_ptr<Block> placedBlock;
    std::shared_ptr<Block> fallingBlock;
    float overlap = 0.0f;
    bool isCuttingBehind = false;
    bool isBonusBlock = false;
    eastl::string axis;
    Scene* scene;

    Layer(Scene* s, const eastl::string& ax, float width, float depth, const glm::vec3& pos, bool bonus = false);
    bool cut(const Block& prevPlacedBlock);
    void clear();
    void tick(float dt, float colorTime);
    void spawnParticles();

private:
    void removeMovingBlock();
    void createPlacedBlock();
    void createFallingBlock(bool isLastFallingBlock = false);
};

class Tower {
public:
    eastl::vector<std::shared_ptr<Layer>> layers;
    std::shared_ptr<Block> baseBlock;
    int direction = 1;
    float speedFactor = 10.0f;

    Tower(Scene* s, eastl::function<void()> onFinish);
    void tick(float dt, float colorTime);
    void place();
    void reset();

private:
    Scene* scene;
    eastl::function<void()> finishCallback;
    void addLayer();
    void reverseDirection();
};

class TowerGame
{
public:
	void initResources();

	void update(float dt);
	void render();

private:
	Camera camera;
	Ref<Scene> scene;
	Ref<SceneRenderer> scene_renderer;
    std::shared_ptr<Tower> tower;

    bool isGameOver = false;
    float colorTime = 0.0f;

    void end();
    void restart();
};