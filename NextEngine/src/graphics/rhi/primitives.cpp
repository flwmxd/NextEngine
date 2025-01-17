#include "graphics/rhi/primitives.h"
#include "graphics/rhi/buffer.h"
#include "graphics/assets/model.h"
#include "graphics/assets/assets.h"
#include "graphics/assets/assets_store.h"

Primitives primitives;

void init_primitives() {
	glm::vec3 vertices_pos[4] = {
			glm::vec3(1,  1, 0),  // top right
			glm::vec3(1, -1, 0),  // bottom right
			glm::vec3(-1, -1, 0),  // bottom left
			glm::vec3(-1,  1, 0)   // top left 
	};

	glm::vec2 tex_coords[4] = {
		glm::vec2(1, 1),   // top right
		glm::vec2(1, 0.0),   // bottom right
		glm::vec2(0, 0),   // bottom left
		glm::vec2(0, 1)    // top left 
	};

	unsigned int indices[6] = {  // note that we start from 0!
		0, 1, 3,  // first Triangle
		1, 2, 3   // second Triangle
	};

	Vertex vertices[4] = {};

	for (int i = 0; i < 4; i++) {
		vertices[i].position = vertices_pos[i];
		vertices[i].tex_coord = tex_coords[i];
        vertices[i].normal = glm::vec3(0,0,1);
        vertices[i].tangent = glm::vec3(0,1,0);
        vertices[i].bitangent = glm::cross(vertices[i].normal, vertices[i].tangent);
	}

	Model& model = *PERMANENT_ALLOC(Model);
	model.materials.data = PERMANENT_ALLOC(sstring);
	model.materials.length = 1;
	model.materials[0] = "default_material";

	model.aabb.max = glm::vec3(1.0, 1.0, 0);
	model.aabb.min = glm::vec3(-1.0, -1.0, 0);

	model.meshes.data = PERMANENT_ALLOC(Mesh);
	model.meshes.length = 1;
	model.meshes[0].buffer[0] = alloc_vertex_buffer(VERTEX_LAYOUT_DEFAULT, 4, vertices, 6, indices);

	primitives.quad = assets.models.assign_handle(std::move(model), true);
	primitives.cube = load_Model("engine/cube.fbx");
	primitives.sphere = load_Model("engine/sphere.fbx");

	primitives.quad_buffer   = get_Model(primitives.quad)->meshes[0].buffer[0];
	primitives.cube_buffer   = get_Model(primitives.cube)->meshes[0].buffer[0];
	primitives.sphere_buffer = get_Model(primitives.sphere)->meshes[0].buffer[0];
	//first_quad = false;
}

bool first_quad = true;
VertexBuffer quad_mesh;

void draw_quad(CommandBuffer& cmd_buffer) {
	glm::mat4 identity(1.0);

	bind_vertex_buffer(cmd_buffer, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4);
	draw_mesh(cmd_buffer, primitives.quad_buffer, frame_alloc_instance_buffer<glm::mat4>(INSTANCE_LAYOUT_MAT4X4, identity));
}
