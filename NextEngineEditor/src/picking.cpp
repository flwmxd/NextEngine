#include "picking.h"
#include "editor.h"
#include "core/io/input.h"
#include "graphics/rhi/draw.h"
#include "ecs/layermask.h"
#include "core/memory/linear_allocator.h"
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "graphics/assets/shader.h"
#include "core/memory/linear_allocator.h"
#include "core/io/logger.h"
#include "graphics/assets/assets.h"
#include "graphics/assets/model.h"
#include "engine/engine.h"
#include "components/camera.h"
#include "graphics/renderer/renderer.h"
#include "core/container/tvector.h"

#define MAX_ENTITIES_PER_NODE 10
#define MAX_PICKING_DEPTH 6

Node* subdivide_BVH(PickingScenePartition& scene_partition, AABB& node_aabb, int depth, int mesh_count, AABB* aabbs, ID* ids) {
	if (mesh_count <= MAX_ENTITIES_PER_NODE || depth >= MAX_PICKING_DEPTH) {
		Node& node = alloc_leaf_node(scene_partition, node_aabb, MAX_PICKING_INSTANCES, mesh_count);
		
		copy_into_node(node, scene_partition.ids, ids);
		copy_into_node(node, scene_partition.aabbs, aabbs);

		return &node;
	}
	else {
		BranchNodeInfo info = alloc_branch_node(scene_partition, node_aabb);
		tvector<AABB> subdivided_aabbs[2];
		tvector<ID> subdivided_ids[2];

		for (int i = 0; i < mesh_count; i++) {
			AABB& aabb = aabbs[i];

			if (bigger_than_leaf(info, aabb)) {
				int offset = elem_offset(scene_partition, info, aabb);
				scene_partition.aabbs[offset] = aabb;
				scene_partition.ids[offset] = ids[i];
			}
			else {
				bool node_index = split_index(info, aabb);
				subdivided_aabbs[node_index].append(aabb);
				subdivided_ids[node_index].append(ids[i]);
			}
		}

		for (int i = 0; i < 2; i++) {
			if (subdivided_aabbs[i].length == 0) continue;
			info.node.child[i] = subdivide_BVH(scene_partition, info.child_aabbs[i], depth + 1, subdivided_aabbs[i].length, subdivided_aabbs[i].data, subdivided_ids[i].data);
			info.node.aabb.update_aabb(info.node.child[i]->aabb);
		}

		bump_allocator(scene_partition, info);
		return &info.node;
	}
}

void build_acceleration_structure(PickingScenePartition& scene_partition, World& world) { 
	AABB world_bounds;

	int occupied = temporary_allocator.occupied;

	vector<AABB> aabbs;
	vector<ID> ids;

	aabbs.allocator = &temporary_allocator;
	ids.allocator = &temporary_allocator;

	for (ID id : world.filter<ModelRenderer, Transform>(ANY_LAYER)) {
		auto model_m = world.by_id<Transform>(id)->compute_model_matrix();
		auto model_renderer = world.by_id<ModelRenderer>(id);

		Model* model = get_Model(model_renderer->model_id);		
		if (model == NULL) continue;

		AABB aabb = model->aabb.apply(model_m);
		world_bounds.update_aabb(aabb);
		aabbs.append(aabb);
		ids.append(id);
	}

	subdivide_BVH(scene_partition, world_bounds, 0, aabbs.length, aabbs.data, ids.data);

	temporary_allocator.occupied = occupied;
}

Ray ray_from_mouse(World& world, ID camera_id, Input& input) {
	glm::vec2 viewport_size = input.region_max - input.region_min;

	Viewport viewport;
	viewport.x = viewport_size.x;
	viewport.y = viewport_size.y;

	update_camera_matrices(world, camera_id, viewport);

	glm::mat4 inv_proj_view = glm::inverse(viewport.proj * viewport.view);

	glm::vec2 mouse_position = input.mouse_position; 
	glm::vec2 mouse_position_clip = mouse_position / viewport_size;
	mouse_position_clip.y = 1.0 - mouse_position_clip.y;
	mouse_position_clip = mouse_position_clip * 2.0f - 1.0f;

	glm::vec4 end = inv_proj_view * glm::vec4(mouse_position_clip, 1, 1.0);
	glm::vec4 start = inv_proj_view * glm::vec4(mouse_position_clip, -1, 1.0);
	
	start /= start.w;
	end /= end.w;

	return Ray(start, glm::normalize(end - start), glm::length(end - start));
}

float max(glm::vec3 vec) {
	return vec.x > vec.y ? (vec.x > vec.z ? vec.x : vec.z) : (vec.y > vec.z ? vec.y : vec.z);
}

//Math from - https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection
float intersect(const AABB& bounds, const Ray &r, glm::vec3& hit) {
	float tmin, tmax, tymin, tymax, tzmin, tzmax;

	tmin = (bounds[r.sign[0]].x - r.orig.x) * r.invdir.x;
	tmax = (bounds[1 - r.sign[0]].x - r.orig.x) * r.invdir.x;
	tymin = (bounds[r.sign[1]].y - r.orig.y) * r.invdir.y;
	tymax = (bounds[1 - r.sign[1]].y - r.orig.y) * r.invdir.y;

	if ((tmin > tymax) || (tymin > tmax))
		return FLT_MAX;
	if (tymin > tmin)
		tmin = tymin;
	if (tymax < tmax)
		tmax = tymax;

	tzmin = (bounds[r.sign[2]].z - r.orig.z) * r.invdir.z;
	tzmax = (bounds[1 - r.sign[2]].z - r.orig.z) * r.invdir.z;

	if ((tmin > tzmax) || (tzmin > tmax))
		return FLT_MAX;
	if (tzmin > tmin)
		tmin = tzmin;
	if (tzmax < tmax)
		tmax = tzmax;

	hit = bounds.min + glm::vec3(tmin, tymin, tzmin);

	return glm::length(hit - r.orig);
}

bool intersect(AABB& bounds, const Ray& r) {
	glm::vec3 hit;
	return intersect(bounds, r, hit) < FLT_MAX;
}

void ray_cast_node(PickingScenePartition& partition, Node* node, const Ray& ray, RayHit& hit) {
	for (int i = 0; i < 2; i++) {
		Node* child = node->child[i];
		if (child && intersect(child->aabb, ray)) ray_cast_node(partition, child, ray, hit);
	}

	float closest_t = hit.t;

	for (int i = node->offset; i < node->offset + node->count; i++) {
		AABB& aabb = partition.aabbs[i];

		glm::vec3 new_hit;
		float dist = intersect(aabb, ray, new_hit);

		if (dist < closest_t && dist < ray.t) {
			hit.position = new_hit;
			hit.id = partition.ids[i];
			hit.t = dist;
		}
	}
}

PickingSystem::PickingSystem() {

}

void PickingSystem::rebuild_acceleration_structure(World& world) {
	build_acceleration_structure(partition, world);
}

bool PickingSystem::ray_cast(const Ray& ray, RayHit& hit) {
	if (partition.count == 0) return false;
	ray_cast_node(partition, &partition.nodes[0], ray, hit);
	return hit.t < FLT_MAX;
}

bool PickingSystem::ray_cast(World& world, Input& input, RayHit& hit) {
	Ray ray = ray_from_mouse(world, get_camera(world, EDITOR_LAYER), input);
	return ray_cast(ray, hit);
}

int PickingSystem::pick(World& world, Input& input) {
	Ray ray = ray_from_mouse(world, get_camera(world, EDITOR_LAYER), input);
	RayHit hit;

	if (ray_cast(ray, hit)) return hit.id;
	else return -1;
}

void render_cube(RenderPass& ctx, model_handle model_handle, material_handle mat_handle, glm::vec3 center, glm::vec3 scale) {
	Transform trans;
	trans.position = center;
	trans.scale = scale * 0.5f;

	draw_mesh(ctx.cmd_buffer, model_handle, mat_handle, trans);
}

struct Material;

void render_node(RenderPass& ctx, material_handle mat, PickingScenePartition& partition, model_handle cube, Node& node, Ray& ray) {
	//render_cube(ctx, mat, cube, (node.aabb.max + node.aabb.min) * 0.5f, node.aabb.max - node.aabb.min);

	for (int i = 0; i < 2; i++) {
		Node* child = node.child[i];
		if (child && intersect(child->aabb, ray)) render_node(ctx, mat, partition, cube, *child, ray);
	}


	for (int i = node.offset; i < node.offset + node.count; i++) {
		AABB& aabb = partition.aabbs[i];
		if (intersect(aabb, ray)) render_cube(ctx, cube, mat, (aabb.max + aabb.min) * 0.5f, aabb.max - aabb.min);
	}
}


#include "graphics/assets/material.h"

void PickingSystem::visualize(World& world, Input& input, RenderPass& ctx) {
	Ray ray = ray_from_mouse(world, get_camera(world, EDITOR_LAYER), input);

	model_handle cube = load_Model("cube.fbx");

	MaterialDesc mat{ load_Shader("shaders/pbr.vert", "shaders/gizmo.frag") };
	mat_vec3(mat, "color", glm::vec3(1.0f, 0.0f, 0.0f));
	mat.draw_state = PolyMode_Wireframe | (5 << WireframeLineWidth_Offset);

	//render_node(ctx, mat, partition, models.get(cube), partition.nodes[0], ray);

	//ray.dir = glm::vec3(0, 0, -1) * world.by_id<Transform>(get_camera(world, EDITOR_LAYER))->rotation;

	//render_cube(ctx, mat, models.get(cube), ray.orig + ray.dir * 5.0f, glm::vec3(0.1f));
	//render_cube(ctx, mat, models.get(cube), ray.orig + ray.dir * 10.0f, glm::vec3(0.1f));
	//render_cube(ctx, mat, models.get(cube), ray.orig + ray.dir * 20.0f, glm::vec3(0.1f));
}

void make_render_outline_state(OutlineRenderState& self) {
	self.outline_shader = load_Shader("shaders/outline.vert", "shaders/outline.frag");

	DrawCommandState object_state = StencilFunc_Always | (0xFF << StencilMask_Offset) | ColorMask_None | DepthFunc_None;

	int outline_width = 3;

	MaterialDesc outline_material{ self.outline_shader };
	outline_material.draw_state = StencilFunc_NotEqual | (0x00 << StencilMask_Offset) | DepthFunc_None | PolyMode_Wireframe | (outline_width << WireframeLineWidth_Offset);

	//this->outline_material = Material(outline_shader);
	//this->outline_material.state = &outline_state;
}

void render_object_selected_outline(OutlineRenderState& outline, World& world, slice<ID> highlighted, RenderPass& ctx) {
	for (ID id : highlighted) {
		Transform* trans = world.by_id<Transform>(id);
		ModelRenderer* model_renderer = world.by_id<ModelRenderer>(id);
		Materials* materials = world.by_id<Materials>(id);

		if (trans && model_renderer && materials) {
			glm::mat4 model_m = trans->compute_model_matrix();

			draw_mesh(ctx.cmd_buffer, model_renderer->model_id, outline.object_material, model_m);
			draw_mesh(ctx.cmd_buffer, model_renderer->model_id, outline.outline_material, model_m);

			Model* model = get_Model(model_renderer->model_id);

			for (Mesh& mesh : model->meshes) {
				material_handle should_be_handle = materials->materials[mesh.material_id];
				//MaterialDesc* should_be = material_desc(should_be_handle);


				//Material* mat = TEMPORARY_ALLOC(Material);
				//mat->params.allocator = &temporary_allocator;
				//*mat = *should_be;

				//mat->state = &object_state;
					
				//ctx.command_buffer.draw(model_m, &mesh.buffer, mat);
				//ctx.command_buffer.draw(model_m, &mesh.buffer, TEMPORARY_ALLOC(Material, outline_material)); //this might be leaking memory!
				//*/
			}
		}
	}
}

