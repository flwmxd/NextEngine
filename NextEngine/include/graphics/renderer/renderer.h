#pragma once

#include "core/handle.h"
#include "ecs/id.h"
#include "core/container/vector.h"
#include "core/container/handle_manager.h"
#include "graphics/renderer/render_feature.h"
#include "graphics/pass/render_pass.h"
#include <glm/mat4x4.hpp>

struct Modules;
struct Window;
struct World;
struct RenderCtx;
struct ShaderConfig;
struct BufferAllocator;
struct RHI;

struct RenderSettings {
	enum GraphicsQuality { Ultra, High, Medium, Low } quality = Ultra;
};

struct ENGINE_API RenderExtension {
	virtual void render_view(struct World&, struct RenderCtx&) {};
	virtual void render(struct World&, struct RenderCtx&) {};
	virtual ~RenderExtension() {};
};

struct ENGINE_API Renderer {
	RenderSettings settings;

	struct RHI& rhi;
	struct BufferAllocator* buffer_manager;
	struct SkyboxSystem* skybox_renderer;
	struct ModelRendererSystem* model_renderer;
	struct GrassRenderSystem* grass_renderer;
	struct TerrainRenderSystem* terrain_renderer;
	struct CullingSystem* culling;

	struct MainPass* main_pass;
	glm::mat4* model_m;

	//void set_render_pass(Pass* pass);
	void update_settings(const RenderSettings&);
	
	void render_view(World& world, RenderCtx&);
	void render_overlay(World& world, RenderCtx&);
	RenderCtx render(World& world, Layermask layermask, uint width, uint height, RenderExtension* ext);

	Renderer(RHI& rhi, Assets& assets, Window&, World&);
	~Renderer();
};

struct RenderCtx {
	RenderExtension* extension;

	Layermask layermask;
	struct CommandBuffer& command_buffer;
	struct Pass* pass;
	struct Skybox* skybox;
	struct DirLight* dir_light;

	glm::vec3 view_pos;
	glm::mat4 projection;
	glm::mat4 view;
	struct Camera* cam = NULL;

	glm::mat4* model_m;

	uint width = 0;
	uint height = 0;
	ENGINE_API void set_shader_scene_params(ShaderConfig&);

	ENGINE_API RenderCtx(struct CommandBuffer&, struct Pass*);
	ENGINE_API RenderCtx(const RenderCtx&, struct CommandBuffer&, struct Pass*);
};

struct ENGINE_API PreRenderParams {
	Layermask layermask;

	PreRenderParams(Layermask layermask);
};