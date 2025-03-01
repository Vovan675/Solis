#include "pch.h"
#include "TowerGame.h"
#include "Editor/EditorDefaultScene.h"

static std::shared_ptr<Model> cube_model;

static const glm::vec3 BASE_BLOCK_SIZE = {4.0f, 1.0f, 4.0f};
static const glm::vec4 COLOR_BASE = {0.27f, 0.27f, 0.27f, 1.0f};
static const glm::vec4 COLOR_PLACED = {0.44f, 0.92f, 0.22f, 1.0f};
static const glm::vec4 COLOR_FALLING = {0.97f, 0.96f, 0.35f, 1.0f};
static const glm::vec4 COLOR_BONUS = {1.0f, 0.0f, 0.0f, 1.0f}; // New: Orange for bonus blocks
static const float INITIAL_SPEED_FACTOR = 10.0f;
static const float MOVING_RANGE = 7.0f;

// --- Block Implementation ---

Block createBlock(Scene* scene, const glm::vec3& size, const glm::vec3& pos, const glm::vec4& color) {
    Entity entity = Model::createEntity(cube_model);
    auto& transform = entity.getTransform();
    transform.scale = size * 0.5f * 0.01f; // Scale to match game units
    transform.position = pos;
    Entity child(entity.getChildren()[0]);
    auto& mr = child.getComponent<MeshRendererComponent>();
    std::shared_ptr<Material> mat = std::make_shared<Material>();
    mat->albedo = color;
    mr.materials[0] = mat;
    Block block(size, pos, color);
    block.entity = entity;
    return block;
}

// --- Layer Implementation ---

Layer::Layer(Scene* s, const std::string& ax, float width, float depth, const glm::vec3& pos, bool bonus) 
    : scene(s), axis(ax), isBonusBlock(bonus) {
    glm::vec3 initPos = pos;
    if (axis == "x") initPos.x = -MOVING_RANGE;
    else initPos.z = -MOVING_RANGE;
    movingBlock = std::make_shared<Block>(createBlock(scene, {width, BASE_BLOCK_SIZE.y, depth}, initPos, isBonusBlock ? COLOR_BONUS : glm::vec4(0.97f, 0.33f, 0.90f, 1.0f)));
}

void Layer::removeMovingBlock() {
    if (movingBlock) {
        scene->destroyEntity(movingBlock->entity);
        movingBlock = nullptr;
    }
}

void Layer::createPlacedBlock() {
    float width = (axis == "x") ? overlap : movingBlock->size.x;
    float depth = (axis == "z") ? overlap : movingBlock->size.z;
    glm::vec3 pos = movingBlock->position;
    float shift = (axis == "x") ? (movingBlock->size.x - overlap) / 2.0f : (movingBlock->size.z - overlap) / 2.0f;
    shift *= (isCuttingBehind ? 1.0f : -1.0f);
    if (axis == "x") pos.x += shift;
    else pos.z += shift;
    placedBlock = std::make_shared<Block>(createBlock(scene, {width, BASE_BLOCK_SIZE.y, depth}, pos, COLOR_PLACED));
    spawnParticles(); // New: Add particles on placement
}

void Layer::createFallingBlock(bool isLastFallingBlock) {
    glm::vec3 size = movingBlock->size;
    glm::vec3 pos = movingBlock->position;
    if (!isLastFallingBlock) {
        size.x = (axis == "x") ? movingBlock->size.x - overlap : movingBlock->size.x;
        size.z = (axis == "z") ? movingBlock->size.z - overlap : movingBlock->size.z;
        float shift = (axis == "x") ? pos.x : pos.z;
        shift += overlap / 2.0f;
        if (isCuttingBehind) shift -= overlap;
        if (axis == "x") pos.x = shift;
        else pos.z = shift;
    }
    fallingBlock = std::make_shared<Block>(createBlock(scene, size, pos, COLOR_FALLING));
}

bool Layer::cut(const Block& prevPlacedBlock) {
    overlap = (axis == "x") 
        ? movingBlock->size.x - std::abs(movingBlock->position.x - prevPlacedBlock.position.x)
        : movingBlock->size.z - std::abs(movingBlock->position.z - prevPlacedBlock.position.z);

    if (overlap <= 0) {
        createFallingBlock(true);
        removeMovingBlock();
        return false;
    }

    isCuttingBehind = (axis == "x") 
        ? movingBlock->position.x < prevPlacedBlock.position.x 
        : movingBlock->position.z < prevPlacedBlock.position.z;

    createPlacedBlock();
    createFallingBlock();
    removeMovingBlock();
    return true;
}

void Layer::tick(float dt, float colorTime) {
    if (fallingBlock) {
        fallingBlock->position.y -= dt * 25.0f;
        fallingBlock->entity.getTransform().position = fallingBlock->position;
    }
    if (movingBlock && !isBonusBlock) { // New: Animate color for regular blocks
        float r = (std::sin(colorTime) + 1.0f) / 2.0f;
        float g = (std::sin(colorTime + 2.0f) + 1.0f) / 2.0f;
        float b = (std::sin(colorTime + 4.0f) + 1.0f) / 2.0f;
        Entity(movingBlock->entity.getChildren()[0]).getComponent<MeshRendererComponent>().materials[0]->albedo = glm::vec4(r, g, b, 1.0f);
    }
}

void Layer::spawnParticles() {
    float count = rand() % 5 + 2;
    for (int i = 0; i < count; ++i) {
        glm::vec3 particlePos = placedBlock->position + placedBlock->size * glm::vec3(
            (rand() % 100) / 100.0f - 0.5f,
            0,
            (rand() % 100) / 100.0f - 0.5f
        );

        float color_rand = (rand() % 100) / 100.0f;
        float size_rand = (rand() % 100) / 100.0f * 0.3 + 0.04;

        particlePos.y += size_rand * 0.5f;

        Block particle = createBlock(scene, {size_rand, size_rand, size_rand}, particlePos, glm::vec4(color_rand, 1.0f, 0.8f, 1.0f));
        auto& transform = particle.entity.getTransform();
        transform.position.y += 0.5f; // Start slightly above
    }
}

void Layer::clear() {
    removeMovingBlock();
    if (placedBlock) scene->destroyEntity(placedBlock->entity);
    if (fallingBlock) scene->destroyEntity(fallingBlock->entity);
    placedBlock = nullptr;
    fallingBlock = nullptr;
}

// --- Tower Implementation ---

Tower::Tower(Scene* s, std::function<void()> onFinish) : scene(s), finishCallback(onFinish) {
    baseBlock = std::make_shared<Block>(createBlock(scene, BASE_BLOCK_SIZE, {0.0f, 0.0f, 0.0f}, COLOR_BASE));
    addLayer();
}

void Tower::addLayer() {
    std::string axis = (layers.empty() || layers.back()->axis == "z") ? "x" : "z";
    glm::vec3 pos = layers.empty() ? glm::vec3(0.0f, BASE_BLOCK_SIZE.y, 0.0f) : layers.back()->placedBlock->position;
    pos.y = (layers.size() + 1) * BASE_BLOCK_SIZE.y;
    float width = layers.empty() ? BASE_BLOCK_SIZE.x : layers.back()->placedBlock->size.x;
    float depth = layers.empty() ? BASE_BLOCK_SIZE.z : layers.back()->placedBlock->size.z;

    // Occasionally spawn a bonus block
    bool isBonus = (rand() % 100) < 20 && !layers.empty() && !layers.back()->isBonusBlock;
    if (isBonus && layers.back()->overlap >= layers.back()->placedBlock->size.x * 0.95f) {
        width *= 1.2f;
        depth *= 1.2f;
    }

    layers.push_back(std::make_shared<Layer>(scene, axis, width, depth, pos, isBonus));
}

void Tower::reverseDirection() {
    direction *= -1;
}

void Tower::tick(float dt, float colorTime) {
    for (auto& layer : layers) {
        layer->tick(dt, colorTime);
    }
    if (!layers.back()->movingBlock) return;

    auto& pos = layers.back()->movingBlock->position;
    float& activeAxis = (layers.back()->axis == "x") ? pos.x : pos.z;
    activeAxis += dt * speedFactor * direction;

    if (activeAxis > MOVING_RANGE) {
        activeAxis = MOVING_RANGE;
        reverseDirection();
    } else if (activeAxis < -MOVING_RANGE) {
        activeAxis = -MOVING_RANGE;
        reverseDirection();
    }
    layers.back()->movingBlock->entity.getTransform().position = pos;
}

static int record = 0;
void Tower::place() {
    const Block& lastBlock = layers.size() > 1 ? *layers[layers.size() - 2]->placedBlock : *baseBlock;
    if (layers.back()->cut(lastBlock)) {
        speedFactor += 0.3f; // New: Increase speed slightly after each placement
        addLayer();
    } else {
        finishCallback();
    }

    if (layers.size() > record)
    {
        record = layers.size();
        CORE_INFO("Max record: {}", record);
    }
}

void Tower::reset() {
    direction = 1;
    speedFactor = INITIAL_SPEED_FACTOR; // New: Reset speed
    for (auto& layer : layers) layer->clear();
    layers.clear();
    addLayer();
}

void TowerGame::initResources()
{
	camera = std::make_shared<Camera>();
    camera->setPosition(glm::vec3(-9, 8, -9));
    camera->setRotation(glm::vec3(35, 45, 0));
	Renderer::setCamera(camera);

	scene = std::make_shared<Scene>();
	Scene::setCurrentScene(scene);
	
	scene_renderer = std::make_shared<SceneRenderer>();
	scene_renderer->setScene(scene);

	//EditorDefaultScene::createScene();
	cube_model = AssetManager::getModelAsset("assets/cube.fbx");

	Entity light = Scene::getCurrentScene()->createEntity("Point Light");
	light.getTransform().setTransform(glm::eulerAngleXYX(3.14 / 4.0, 3.14 / 4.0, 0.0));
	auto &light_component = light.addComponent<LightComponent>();
	light_component.setType(LIGHT_TYPE_DIRECTIONAL);
	light_component.color = glm::vec3(253.0f / 255, 251.0f / 255, 211.0f / 255);
	light_component.intensity = 1.0f;
	light_component.radius = 10.0f;

	// Base
	Entity entity = Model::createEntity(cube_model);
	auto &transform = entity.getTransform();
	transform.scale = glm::vec3(0.01, 0.001, 0.01);
	transform.position = glm::vec3(0.0, -1.01, 0.0);

    tower = std::make_shared<Tower>(scene.get(), [this]() { end(); });
}

void TowerGame::update(float dt)
{
	bool mouse_pressed = gInput.isMouseKeyDown(GLFW_MOUSE_BUTTON_1);

	auto swapchain = gDynamicRHI->getSwapchainTexture(gDynamicRHI->current_frame);
	camera->setAspect(swapchain->getWidth() / (float)swapchain->getHeight());
	camera->update(dt, gInput.getMousePosition(), mouse_pressed);
	camera->updateMatrices();

	Renderer::updateDefaultUniforms(dt);
	scene_renderer->ssao_renderer.ubo_raw_pass.near_plane = camera->getNear();
	scene_renderer->ssao_renderer.ubo_raw_pass.far_plane = camera->getFar();

	static bool prev_down = false;
	bool current_down = gInput.isMouseKeyDown(GLFW_MOUSE_BUTTON_2);
	if (current_down && !prev_down && !isGameOver)
	{
        tower->place();

        camera->setPosition(glm::vec3(-9, 8 + tower->layers.size(), -9));
	}

    prev_down = current_down;

    if (gInput.isKeyDown(GLFW_KEY_R)) { // New: Restart with 'R' key
        restart();
    }

    colorTime += dt; // New: Update color animation time
    tower->tick(dt, colorTime);
}

void TowerGame::render()
{
	scene_renderer->render(camera, gDynamicRHI->getSwapchainTexture(gDynamicRHI->current_frame));
}

void TowerGame::end() {
    isGameOver = true;
}

void TowerGame::restart() {
    tower->reset();
    camera->setPosition(glm::vec3(-9, 8, -9));
    isGameOver = false;
}