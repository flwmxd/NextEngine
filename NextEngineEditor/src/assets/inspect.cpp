#include <graphics/assets/assets.h>
#include <graphics/assets/shader.h>
#include <graphics/rhi/pipeline.h>
#include <imgui/imgui.h>
#include "assets/node.h"
#include "assets/explorer.h"
#include "assets/inspect.h"
#include "custom_inspect.h"
#include "diffUtil.h"

#include "generated.h"
#include "editor.h"

struct AssetTab;
struct Editor;

void edit_color(glm::vec4& color, string_view name, glm::vec2 size) {

	ImVec4 col(color.x, color.y, color.z, color.w);
	if (ImGui::ColorButton(name.c_str(), col, 0, ImVec2(size.x, size.y))) {
		ImGui::OpenPopup(name.c_str());
	}

	color = glm::vec4(col.x, col.y, col.z, col.w);

	if (ImGui::BeginPopup(name.c_str())) {
		ImGui::ColorPicker4(name.c_str(), &color.x);
		ImGui::EndPopup();
	}
}

void edit_color(glm::vec3& color, string_view name, glm::vec2 size) {
	glm::vec4 color4 = glm::vec4(color, 1.0f);
	edit_color(color4, name, size);
	color = color4;
}

void channel_image(uint& image, string_view name) {
	if (image == INVALID_HANDLE) { ImGui::Image(default_textures.white, ImVec2(200, 200)); }
	else ImGui::Image({ image }, ImVec2(200, 200));

	if (ImGui::IsMouseDragging() && ImGui::IsItemHovered()) {
		image = INVALID_HANDLE;
	}

	accept_drop("DRAG_AND_DROP_IMAGE", &image, sizeof(texture_handle));
	ImGui::SameLine();

	ImGui::Text(name.c_str());
	ImGui::SameLine(ImGui::GetWindowWidth() - 300);
}

void texture_properties(TextureAsset* tex, Editor& editor) {

}

MaterialDesc base_shader_desc(shader_handle shader) {
	MaterialDesc desc{ shader };
	desc.draw_state = Cull_None;

	mat_channel3(desc, "diffuse", glm::vec3(1.0f));
	mat_channel1(desc, "metallic", 0.0f);
	mat_channel1(desc, "roughness", 0.5f);
	mat_channel1(desc, "normal", 1.0f, default_textures.normal);
	mat_vec2(desc, "tiling", glm::vec2(5.0f));
	//mat_vec2(desc, "tiling", glm::vec2(1, 1));

	return desc;
}

void inspect_material_params(Editor& editor, material_handle handle, MaterialDesc* material) {
	static DiffUtil diff_util;
	static MaterialDesc copy;
	static material_handle last_asset = { INVALID_HANDLE };

	if (handle.id != last_asset.id) {
		diff_util.id = handle.id;
		begin_diff(diff_util, material, &copy, get_MaterialDesc_type());
	}

	bool slider_active = false;

	for (auto& param : material->params) {
		const char* name = param.name.data;

		if (param.type == Param_Vec2) {
			ImGui::PushItemWidth(200.0f);
			ImGui::InputFloat2(name, &param.vec2.x);
		}
		if (param.type == Param_Vec3) {
			edit_color(param.vec3, name);

			ImGui::SameLine();
			ImGui::Text(name);
		}
		if (param.type == Param_Image) {
			channel_image(param.image, param.name);
			ImGui::SameLine();
			ImGui::Text(name);
		}
		if (param.type == Param_Int) {
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.3f);
			ImGui::InputInt(name, &param.integer);
		}

		if (param.type == Param_Float) {
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.3f);
			ImGui::SliderFloat(name, &param.real, -1.0f, 1.0f);

			slider_active |= ImGui::IsItemActive();
		}

		if (param.type == Param_Channel3) {
			channel_image(param.image, name);
			edit_color(param.vec3, name, glm::vec2(50, 50));
			slider_active |= ImGui::IsItemActive();
		}

		if (param.type == Param_Channel2) {
			channel_image(param.image, name);
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.3f);
			ImGui::InputFloat2(tformat("##", name).c_str(), &param.vec2.x);
		}

		if (param.type == Param_Channel1) {
			channel_image(param.image, name);
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.3f);
			ImGui::SliderFloat(tformat("##", name).c_str(), &param.real, 0, 1.0f);
			slider_active |= ImGui::IsItemActive();
		}
	}

	if (slider_active) {
		submit_diff(editor.actions.frame_diffs, diff_util, "Material Property");
	}
	else {
		end_diff(editor.actions, diff_util, "Material Property");
	}
}

void material_properties(MaterialAsset* mat_asset, Editor& editor, AssetTab& asset_tab) {
	AssetPreviewResources& previewer = asset_tab.preview_resources;
	
	auto& name = mat_asset->name;

	render_name(name, asset_tab.explorer.default_font);

	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Material");

	MaterialDesc& desc = mat_asset->desc;
	if (false) { //todo check if handle becomes invalid, then again this should never happen 
		ImGui::Text("Material Handle is INVALID");
	}

	ShaderInfo* info = shader_info(desc.shader);
	if (info == NULL) {
		ImGui::Text("Shader Handle is INVALID!");
	}

	if (ImGui::Button(info->ffilename.data)) {
		ImGui::OpenPopup("StandardShaders");
	}


	if (accept_drop("DRAG_AND_DROP_SHADER", &desc.shader, sizeof(shader_handle))) {
		set_params_for_shader_graph(asset_tab, desc.shader);
	}


	if (ImGui::BeginPopup("StandardShaders")) {
		if (ImGui::Button("shaders/pbr.frag")) {
			shader_handle new_shad = global_shaders.pbr;
			if (new_shad.id != desc.shader.id) {
				desc = base_shader_desc(new_shad);
				//set_base_shader_params(assets, desc, new_shad);
				//material->set_vec2(shader_manager, "transformUVs", glm::vec2(1, 1));
			}
		}

		if (ImGui::Button("shaders/tree.frag")) {
			shader_handle new_shad = global_shaders.grass;
			if (new_shad.id != desc.shader.id) {
				//todo enable undo/redo for switching shaders

				desc = base_shader_desc(new_shad);
				desc.draw_state = Cull_None;
				mat_float(desc, "cutoff", 0.5f);
			}
		}

		if (ImGui::Button("shaders/paralax_pbr.frag")) {
			shader_handle new_shad = load_Shader("shaders/pbr.vert", "shaders/paralax_pbr.frag");

			if (new_shad.id != desc.shader.id) {
				desc = base_shader_desc(new_shad);
				mat_channel1(desc, "height", 1.0);
				mat_channel1(desc, "ao", 1.0);
				mat_int(desc, "steps", 5);
				mat_float(desc, "depth_scale", 1);
				mat_vec2(desc, "transformUVs", glm::vec2(1));
			}
		}

		ImGui::EndPopup();
	}

	inspect_material_params(editor, mat_asset->handle, &desc);

	rot_preview(previewer, mat_asset->rot_preview);
	render_preview_for(previewer, *mat_asset);
}

void model_properties(ModelAsset* mod_asset, Editor& editor, AssetTab& self) {
	Model* model = get_Model(mod_asset->handle);

	render_name(mod_asset->name, self.explorer.default_font);

	DiffUtil diff_util;
	begin_tdiff(diff_util, &mod_asset->trans, get_Transform_type());

	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Transform");
	ImGui::InputFloat3("position", &mod_asset->trans.position.x);
	get_on_inspect_gui("glm::quat")(&mod_asset->trans.rotation, "rotation", editor);
	ImGui::InputFloat3("scale", &mod_asset->trans.scale.x);

	end_diff(editor.actions, diff_util, "Properties Material");

	if (ImGui::Button("Apply")) {
		begin_gpu_upload();
		load_Model(mod_asset->handle, mod_asset->path, compute_model_matrix(mod_asset->trans));
		end_gpu_upload();

		mod_asset->rot_preview.rot_deg = glm::vec2();
		mod_asset->rot_preview.current = glm::vec2();
		mod_asset->rot_preview.previous = glm::vec2();
		mod_asset->rot_preview.rot = glm::quat();
	}

	if (ImGui::CollapsingHeader("LOD")) {
		float height = 200.0f;
		float padding = 10.0f;
		float cull_distance = model->lod_distance.last();
		float avail_width = ImGui::GetContentRegionAvail().x - padding * 4;

		float last_dist = 0;

		auto draw_list = ImGui::GetForegroundDrawList();

		glm::vec2 cursor_pos = glm::vec2(ImGui::GetCursorScreenPos()) + glm::vec2(padding);

		ImGui::SetCursorPos(glm::vec2(ImGui::GetCursorPos()) + glm::vec2(0, height + padding * 2));

		ImU32 colors[MAX_MESH_LOD] = {
			ImColor(245, 66, 66),
			ImColor(245, 144, 66),
			ImColor(245, 194, 66),
			ImColor(170, 245, 66),
			ImColor(66, 245, 111),
			ImColor(66, 245, 212),
			ImColor(66, 126, 245),
			ImColor(66, 69, 245),
		};

		//todo logic could be simplified
		static int dragging = -1;
		static glm::vec2 last_drag_delta;
		glm::vec2 drag_delta = glm::vec2(ImGui::GetMouseDragDelta()) - last_drag_delta;

		uint lod_count = model->lod_distance.length;

		if (!ImGui::IsMouseDragging()) {
			dragging = -1;
			last_drag_delta = glm::vec2();
			drag_delta = glm::vec2();
		}

		for (uint i = 0; i < lod_count; i++) {
			float dist = model->lod_distance[i];
			float offset = last_dist / cull_distance * avail_width;
			float width = avail_width * (dist - last_dist) / cull_distance;

			draw_list->AddRectFilled(cursor_pos + glm::vec2(offset, 0), cursor_pos + glm::vec2(offset + width, height), colors[i]);

			char buffer[100];
			sprintf_s(buffer, "LOD %i - %.1fm", i, dist);

			draw_list->AddText(cursor_pos + glm::vec2(offset + padding, padding), ImColor(255, 255, 255), buffer);

			last_dist = dist;

			bool last = i + 1 == lod_count;
			bool is_dragged = i == dragging;
			if (!last && (is_dragged || ImGui::IsMouseHoveringRect(cursor_pos + glm::vec2(offset + width - padding, 0), cursor_pos + glm::vec2(offset + width + padding, height)))) {
				draw_list->AddRectFilled(cursor_pos + glm::vec2(offset + width - padding, 0), cursor_pos + glm::vec2(offset + width + padding, height), ImColor(255, 255, 255));

				model->lod_distance[i] += drag_delta.x * cull_distance / avail_width;
				dragging = i;
			}
		}

		float last_cull_distance = cull_distance;
		ImGui::InputFloat("Culling distance", &cull_distance);
		if (last_cull_distance != cull_distance) {
			for (uint i = 0; i < lod_count - 1; i++) {
				model->lod_distance[i] *= cull_distance / last_cull_distance;
			}

			model->lod_distance.last() = cull_distance;
		}

		last_drag_delta = ImGui::GetMouseDragDelta();

		mod_asset->lod_distance = (slice<float>)model->lod_distance;
	}

	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Materials");

	if (model->materials.length != mod_asset->materials.length) {
		mod_asset->materials.resize(model->materials.length);
	}

	for (int i = 0; i < mod_asset->materials.length; i++) {
		string_buffer prefix = tformat(model->materials[i], " : ");

		get_on_inspect_gui("Material")(&mod_asset->materials[i], prefix, editor);
	}

	end_diff(editor.actions, diff_util, "Asset Properties");

	rot_preview(self.preview_resources, mod_asset->rot_preview);
	render_preview_for(self.preview_resources, *mod_asset);
}


void asset_properties(AssetNode& node, Editor& editor, AssetTab& tab) {
	if (node.type == AssetNode::Texture) texture_properties(&node.texture, editor);
	if (node.type == AssetNode::Shader) shader_graph_properties(node, editor, tab); //todo fix these signatures
	if (node.type == AssetNode::Model) model_properties(&node.model, editor, tab);
	if (node.type == AssetNode::Material) material_properties(&node.material, editor, tab);
}