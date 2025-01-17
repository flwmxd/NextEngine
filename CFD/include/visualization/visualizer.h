#pragma once

#include "engine/handle.h"
#include "core/container/tvector.h"
#include "graphics/rhi/buffer.h"
#include "ecs/id.h"
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include "core/math/vec4.h"

struct CommandBuffer;
struct CFDVolume;
struct World;
struct CFDVisualization;
struct CFDRenderBackend;
struct RenderPass;
struct FrameData;
struct Renderer;

CFDVisualization* make_cfd_visualization(CFDRenderBackend&);
void build_vertex_representation(CFDVisualization& visualization, CFDVolume&, vec4 plane, bool rebuild);
void render_cfd_mesh(CFDVisualization& visualization, CommandBuffer& cmd_buffer);
