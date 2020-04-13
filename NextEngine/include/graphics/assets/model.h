#pragma once

#include "core/reflection.h"
#include "core/container/sstring.h"
#include "core/container/slice.h"
#include "core/container/handle_manager.h"
#include "core/handle.h"
#include "graphics/rhi/buffer.h"
#include "graphics/culling/aabb.h"
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 tex_coord;
	glm::vec3 tangent;
	glm::vec3 bitangent;

	REFLECT(ENGINE_API)
};

struct Mesh {
	VertexBuffer buffer;
	slice<Vertex> vertices;
	slice<uint> indices;
	AABB aabb;
	uint material_id;

	REFLECT(ENGINE_API)
};

struct Model {
	sstring path;
	slice<Mesh> meshes;
	slice<sstring> materials;
	AABB aabb;

	REFLECT(ENGINE_API)
};

struct ModelRenderer {
	bool visible = true;
	model_handle model_id;

	REFLECT(ENGINE_API)
};

struct Level;
struct BufferAllocator;

void load_assimp(Model* model, BufferAllocator& buffer_allocator, string_view real_path, const glm::mat4& apply_transform);