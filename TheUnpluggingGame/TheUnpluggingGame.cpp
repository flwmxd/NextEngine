// TheUnpluggingGame.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "game.h"

#include <ecs/system.h>
#include <components/transform.h>
#include <model/model.h>
#include <graphics/rhi.h>

extern "C" {
	void register_components_and_systems(World& world) {
		register_default_systems_and_components(world);

		Handle<Model> sphere = load_Model("cube.fbx");
		Handle<Shader> shader = load_Shader("shaders/pbr.vert", "shaders/pbr.frag");


		Handle<Material> mat_handle = RHI::material_manager.make({
			"DefaultMaterial",
			shader,
			{},
			&default_draw_state
		});

		/*
		for (int x = 0; x < 1000; x++) {
			for (int y = 0; y < 1000; y++) {
				ID id = world.make_ID();
				Entity* e = world.make<Entity>(id);
				e->layermask = game_layer;

				Transform* trans = world.make<Transform>(id);
				trans->position.x = x;
				trans->position.z = y ;
				trans->position.y = 2 * std::sin(x / 5.0f) + 2 * std::sin(y / 5.0f);
				trans->scale = glm::vec3(0.5);

				ModelRenderer* renderer = world.make<ModelRenderer>(id);
				renderer->model_id = sphere;

				Materials* materials = world.make<Materials>(id);
				materials->materials.append(mat_handle);

				StaticTransform* static_trans = world.make<StaticTransform>(id);
				static_trans->model_matrix = trans->compute_model_matrix();
			}
		}
		*/
	}
}