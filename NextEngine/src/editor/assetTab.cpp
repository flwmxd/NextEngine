#include "stdafx.h"
#include "editor/assetTab.h"
#include "editor/editor.h"
#include "graphics/rhi.h"
#include "logger/logger.h"
#include <commdlg.h>
#include <thread>
#include "graphics/window.h"
#include "core/vfs.h"
#include "model/model.h"
#include "graphics/texture.h"
#include "graphics/shader.h"
#include "graphics/draw.h"
#include "components/transform.h"
#include "components/camera.h"
#include <algorithm>
#include <locale>
#include <codecvt>
#include "graphics/primitives.h"
#include "graphics/materialSystem.h"
#include "editor/custom_inspect.h"
#include "core/input.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <WinBase.h>

#include <iostream>

REFLECT_STRUCT_BEGIN(AssetFolder)
REFLECT_STRUCT_MEMBER(contents)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(TextureAsset)
REFLECT_STRUCT_MEMBER(name)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(ShaderAsset)
REFLECT_STRUCT_MEMBER(name)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(ModelAsset)
REFLECT_STRUCT_MEMBER(name)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(MaterialAsset)
REFLECT_STRUCT_MEMBER(name)
REFLECT_STRUCT_END()

DEFINE_COMPONENT_ID(AssetFolder, 0)
DEFINE_COMPONENT_ID(TextureAsset, 1)
DEFINE_COMPONENT_ID(ShaderAsset, 2)
DEFINE_COMPONENT_ID(ModelAsset, 3)
DEFINE_COMPONENT_ID(MaterialAsset, 4)

void AssetTab::register_callbacks(Window& window, Editor& editor) {
	
}

void render_name(std::string& name, ImFont* font) {
	char buff[50];
	memcpy(buff, name.c_str(), name.size() + 1);

	ImGui::PushItemWidth(-1);
	ImGui::PushID((void*)&name);
	ImGui::PushFont(font);
	ImGui::InputText("##", buff, 50);
	ImGui::PopItemWidth();
	ImGui::PopFont();
	name = buff;
	ImGui::PopID();

}

wchar_t* to_wide_char(const char* orig);

bool endsWith(const std::string& str, const std::string& suffix)
{
	return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

bool startsWith(const std::string& str, const std::string& prefix)
{
	return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
}

bool filtered(AssetTab& self, const std::string& name) {
	return !startsWith(name, self.filter);
}

void render_asset(ImTextureID texture_id, std::string& name, AssetTab& tab, ID id, const char* drag_drop_type = "", void* data = NULL) {
	if (filtered(tab, name)) return;
	
	bool selected = id == tab.selected;
	if (selected) ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3);

	bool is_selected = ImGui::ImageButton((ImTextureID)texture_id, ImVec2(128, 128), ImVec2(0, 1), ImVec2(1, 0));
		
	if (selected) ImGui::PopStyleVar(1);

	if (data && ImGui::BeginDragDropSource()) {
		ImGui::Image((ImTextureID)texture_id, ImVec2(128, 128), ImVec2(0, 1), ImVec2(1, 0));
		ImGui::SetDragDropPayload(drag_drop_type, data, sizeof(void*));
		ImGui::EndDragDropSource();
	}
	else if (is_selected) {
		tab.selected = id;
	}

	render_name(name, tab.filename_font);

	ImGui::NextColumn();
}

template<typename AssetType>
void move_to_folder(AssetTab& self, ID folder_handle, unsigned int internal_handle) {
	World& world = self.assets;
	auto folder = world.by_id<AssetFolder>(self.current_folder);
	auto move_to = world.by_id<AssetFolder>(folder_handle);

	vector<ID> new_contents;
	new_contents.reserve(folder->contents.length - 1);

	for (int i = 0; i < folder->contents.length; i++) {
		auto handle = folder->contents[i];
		auto mat = world.by_id<AssetType>(handle);
		if (mat && mat->handle.id == internal_handle) {
			move_to->contents.append(handle);
		} else {
			new_contents.append(folder->contents[i]);
		}
	}

	folder->contents = std::move(new_contents);
}

int accept_move_to_folder(const char* drop_type) {
	if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(drop_type)) {
		return ((Handle<unsigned int>*)payload->Data)->id;
	}

	return -1;
}

struct DefferedMoveFolder {
	int mat_moved_to_folder = -1;
	int image_moved_to_folder = -1;
	int model_moved_to_folder = -1;
	int folder_moved_to_folder = -1;
	int moved_to_folder = -1;

	void accept_drop(ID folder_handle) {
		if (ImGui::BeginDragDropTarget()) {
			mat_moved_to_folder = accept_move_to_folder("DRAG_AND_DROP_MATERIAL");
			model_moved_to_folder = accept_move_to_folder("DRAG_AND_DROP_MODEL");
			image_moved_to_folder = accept_move_to_folder("DRAG_AND_DROP_IMAGE");
			folder_moved_to_folder = accept_move_to_folder("DRAG_AND_DROP_FOLDER");

			moved_to_folder = folder_handle;
			ImGui::EndDragDropTarget();
		}
	}

	void apply(AssetTab& asset_tab) {
		if (moved_to_folder != -1) {
			if (mat_moved_to_folder != -1) move_to_folder<MaterialAsset>(asset_tab, moved_to_folder, mat_moved_to_folder);
			if (model_moved_to_folder != -1) move_to_folder<ModelAsset>(asset_tab, moved_to_folder, model_moved_to_folder);
			if (image_moved_to_folder != -1) move_to_folder<TextureAsset>(asset_tab, moved_to_folder, image_moved_to_folder);
			if (folder_moved_to_folder != -1) {
				asset_tab.assets.by_id<AssetFolder>(folder_moved_to_folder - 1)->owner = moved_to_folder;
				move_to_folder<AssetFolder>(asset_tab, moved_to_folder, folder_moved_to_folder);
			}
		}
	}
};

void render_assets(Editor& editor, AssetTab& asset_tab, const std::string& filter) {
	ID folder_handle = asset_tab.current_folder;
	World& world = asset_tab.assets;
	auto folder = world.by_id<AssetFolder>(folder_handle);
	
	ImGui::PushStyleColor(ImGuiCol_Column, ImVec4(0.16, 0.16, 0.16, 1));
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.16, 0.16, 0.16, 1));

	ImGui::Columns(16, 0, false);

	DefferedMoveFolder deffered_move_folder;

	for (auto handle : folder->contents) {
		auto tex = world.by_id<TextureAsset>(handle);
		auto shad = world.by_id<ShaderAsset>(handle);
		auto mod = world.by_id<ModelAsset>(handle);
		auto mat = world.by_id<MaterialAsset>(handle);
		auto folder = world.by_id<AssetFolder>(handle);

		if (tex) {
			render_asset((ImTextureID)texture::id_of(tex->handle), tex->name, asset_tab, handle, "DRAG_AND_DROP_IMAGE", &tex->handle);
		}

		if (shad) { 
			render_asset(editor.get_icon("shader"), shad->name, asset_tab, handle);
		}

		if (mod) {
			render_asset((ImTextureID)texture::id_of(mod->rot_preview.preview), mod->name, asset_tab, handle, "DRAG_AND_DROP_MODEL", &mod->handle);
		}

		if (mat) { 
			render_asset((ImTextureID)texture::id_of(mat->rot_preview.preview), mat->name, asset_tab, handle, "DRAG_AND_DROP_MATERIAL", &mat->handle);
		}

		if (folder && !filtered(asset_tab, folder->name)) {
			folder->handle = { handle + 1 };

			ImGui::PushID(handle);
			if (ImGui::ImageButton((ImTextureID)editor.get_icon("folder"), ImVec2(128, 128), ImVec2(0, 1), ImVec2(1, 0))) {
				asset_tab.current_folder = handle;
			}
			ImGui::PopID();

			if (ImGui::BeginDragDropSource()) {
				ImGui::SetDragDropPayload("DRAG_AND_DROP_FOLDER", &folder->handle, sizeof(void*));
				ImGui::Image((ImTextureID)editor.get_icon("folder"), ImVec2(128, 128), ImVec2(0, 1), ImVec2(1, 0));
				ImGui::EndDragDropSource();
			}

			deffered_move_folder.accept_drop(handle);

			render_name(folder->name, asset_tab.filename_font);
			ImGui::NextColumn();
		}
	}

	ImGui::Columns(1);
	ImGui::PopStyleColor(2);

	deffered_move_folder.apply(asset_tab);
}

void asset_properties(TextureAsset* tex, Editor& editor, World& world, AssetTab& self, RenderParams& params) {

}

//Materials

void asset_properties(ShaderAsset* tex, Editor& editor, World& world, AssetTab& self, RenderParams& params) {

}

std::wstring open_dialog(Editor& editor) {
	wchar_t filename[MAX_PATH];

	OPENFILENAME ofn;
	memset(&filename, 0, sizeof(filename));
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);

	ofn.hwndOwner = glfwGetWin32Window(editor.window.window_ptr);
	ofn.lpstrFilter = L"All Files\0*.*\0";
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;
	ofn.lpstrDefExt = L"";
	ofn.lpstrInitialDir = to_wide_char(Level::asset_folder_path.c_str());

	bool success = GetOpenFileName(&ofn);
	
	delete ofn.lpstrInitialDir;

	if (success) {
		return std::wstring(filename);
	}

	return L"";
}

void create_texture_for_preview(Handle<Texture>& preview, AssetTab& self) {
	unsigned int texture_id;
	if (preview.id == INVALID_HANDLE) {
		glGenTextures(1, &texture_id);
		glBindTexture(GL_TEXTURE_2D, texture_id);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, self.preview_fbo.width, self.preview_fbo.height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

		preview = RHI::texture_manager.make({ "asset preview", texture_id });
	}
	else {
		texture::bind_to(preview, 0);
	}

	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, self.preview_fbo.width, self.preview_fbo.height);
	glGenerateMipmap(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, 0);
}

RenderParams create_preview_command_buffer(CommandBuffer& cmd_buffer, RenderParams& old_params, AssetTab& self, Camera* cam, World& world) {
	RenderParams render_params = old_params;
	render_params.width = self.preview_fbo.width;
	render_params.height = self.preview_fbo.height;
	render_params.command_buffer = &cmd_buffer;
	render_params.cam = cam;

	cam->fov = 60;
	cam->update_matrices(world, render_params);

	return render_params;
}

Handle<Material> create_default_material() { //todo move to materialSystem
	Handle<Shader> pbr = load_Shader("shaders/pbr.vert", "shaders/pbr.frag");

	return RHI::material_manager.make({
		"Empty",
		pbr,
		{
			make_Param_Image(location(pbr, "material.diffuse"), load_Texture("solid_white.png")),
			make_Param_Image(location(pbr, "material.roughness"), load_Texture("solid_white.png")),
			make_Param_Image(location(pbr, "material.metallic"), load_Texture("solid_white.png")),
			make_Param_Image(location(pbr, "material.normal"), load_Texture("normal.jpg")),
			make_Param_Vec2(location(pbr, "transformUVs"), glm::vec2(1, 1))
		},
		&default_draw_state
		});
}

void render_preview_to_buffer(AssetTab& self, RenderParams& params, CommandBuffer& cmd_buffer, Handle<Texture>& preview, World& world) {
	self.preview_fbo.bind();
	self.preview_fbo.clear_color(glm::vec4(0, 0, 0, 1));
	self.preview_fbo.clear_depth(glm::vec4(0, 0, 0, 1));

	Handle<Shader> tone_map = load_Shader("shaders/screenspace.vert", "shaders/preview.frag");

	params.command_buffer->submit_to_gpu(world, params);

	self.preview_fbo.unbind();
	self.preview_tonemapped_fbo.bind();
	self.preview_tonemapped_fbo.clear_color(glm::vec4(0, 0, 0, 1));
	self.preview_tonemapped_fbo.clear_depth(glm::vec4(0, 0, 0, 1));

	glm::mat4 identity(1.0);

	shader::bind(tone_map);
	shader::set_int(tone_map, "frameMap", 0);
	shader::set_mat4(tone_map, "model", identity);
	texture::bind_to(self.preview_map, 0);

	render_quad();

	create_texture_for_preview(preview, self);

	self.preview_tonemapped_fbo.unbind();
}

void render_preview_for(World& world, AssetTab& self, ModelAsset& asset, RenderParams& old_params) {
	ID id = world.make_ID();
	Entity* entity = world.make<Entity>(id);
	Transform* trans = world.make<Transform>(id);
	Camera* cam = world.make<Camera>(id);

	Transform trans_of_model;
	trans_of_model.rotation = asset.rot_preview.rot;

	glm::mat4 model_m = trans_of_model.compute_model_matrix();
	
	Model* model = RHI::model_manager.get(asset.handle);
	AABB aabb;
	for (Mesh& sub_mesh : model->meshes) {
		aabb.update_aabb(sub_mesh.aabb);
	}

	glm::vec3 center = (aabb.max + aabb.min) / 2.0f;
	float radius = 0;

	glm::vec4 verts[8];
	aabb_to_verts(&aabb, verts);
	for (int i = 0; i < 8; i++) {
		radius = std::max(radius, glm::length(glm::vec3(verts[i]) - center));
	}
	
	double fov = glm::radians(60.0f);

	trans->position = center;
	trans->position.z += (radius) / glm::tan(fov / 2.0);

	CommandBuffer cmd_buffer;
	RenderParams params = create_preview_command_buffer(cmd_buffer, old_params, self, cam, world);

	model->render(0, &model_m, asset.materials, params);

render_preview_to_buffer(self, params, cmd_buffer, asset.rot_preview.preview, world);

world.free_by_id(id);
}

void render_preview_for(World& world, AssetTab& self, MaterialAsset& asset, RenderParams& old_params) {
	ID id = world.make_ID();
	Entity* entity = world.make<Entity>(id);
	Transform* trans = world.make<Transform>(id);
	trans->position.z = 2.5;
	Camera* cam = world.make<Camera>(id);

	Model* model = RHI::model_manager.get(load_Model("sphere.fbx"));

	Transform trans_of_sphere;
	trans_of_sphere.rotation = asset.rot_preview.rot;

	glm::mat4 model_m = trans_of_sphere.compute_model_matrix();

	vector<Handle<Material>> materials = { asset.handle };

	CommandBuffer cmd_buffer;
	RenderParams render_params = create_preview_command_buffer(cmd_buffer, old_params, self, cam, world);

	model->render(0, &model_m, materials, render_params);

	render_preview_to_buffer(self, render_params, cmd_buffer, asset.rot_preview.preview, world);

	world.free_by_id(id);
}

void rot_preview(RotatablePreview& self) {
	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Preview");

	ImGui::Image((ImTextureID)texture::id_of(self.preview), ImVec2(512, 512), ImVec2(0, 1), ImVec2(1, 0));
	
	if (ImGui::IsItemHovered() && ImGui::IsMouseDragging()) {
		self.previous = self.current;
		self.current = glm::vec2(ImGui::GetMouseDragDelta().x, ImGui::GetMouseDragDelta().y);

		glm::vec2 diff = self.current - self.previous;

		self.rot_deg += diff;
		self.rot = glm::quat(glm::vec3(glm::radians(self.rot_deg.y), 0, 0)) * glm::quat(glm::vec3(0, glm::radians(self.rot_deg.x), 0));
	}
	else {
		self.previous = glm::vec2(0);
	}
}

void asset_properties(MaterialAsset* mat_asset, Editor& editor, World& world, AssetTab& self, RenderParams& params) {
	Material* material = RHI::material_manager.get(mat_asset->handle);
	void* data = material;

	reflect::TypeDescriptor_Struct* material_type = (reflect::TypeDescriptor_Struct*)reflect::TypeResolver<Material>::get();

	auto& name = mat_asset->name;

	render_name(name, self.default_font);

	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Material");

	for (auto field : material_type->members) {
		if (field.name == "name");
		else if (field.name == "params") {
			//if (ImGui::TreeNode("params")) {
			for (auto& param : material->params) {
				auto shader = RHI::shader_manager.get(material->shader);
				auto& uniform = shader->uniforms[param.loc.id];

				if (param.type == Param_Vec2) {
					ImGui::InputFloat2(uniform.name.c_str(), &param.vec2.x);
				}
				if (param.type == Param_Vec3) {
					ImGui::ColorPicker3(uniform.name.c_str(), &param.vec3.x);
				}
				if (param.type == Param_Image) {
					ImGui::Image((ImTextureID)texture::id_of(param.image), ImVec2(200, 200));

					accept_drop("DRAG_AND_DROP_IMAGE", &param.image, sizeof(Handle<Texture>));

					ImGui::SameLine();
					ImGui::Text(uniform.name.c_str());
				}
				if (param.type == Param_Int) {
					ImGui::InputInt(uniform.name.c_str(), &param.integer);
				}
			}
		}
		else {
			field.type->render_fields((char*)data + field.offset, field.name, world);
		}
	}

	rot_preview(mat_asset->rot_preview);

	render_preview_for(world, self, *mat_asset, params);
}

void asset_properties(ModelAsset* mod_asset, Editor& editor, World& world, AssetTab& self, RenderParams& params) {
	Model* model = RHI::model_manager.get(mod_asset->handle);

	render_name(mod_asset->name, self.default_font);

	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Transform");
	ImGui::InputFloat3("position", &mod_asset->trans.position.x);
	render_fields_primitive(&mod_asset->trans.rotation, "rotation");
	ImGui::InputFloat3("scale", &mod_asset->trans.scale.x);

	if (ImGui::Button("Apply")) {
		model->load_in_place(mod_asset->trans.compute_model_matrix());
		mod_asset->rot_preview.rot_deg = glm::vec2();
		mod_asset->rot_preview.current = glm::vec2();
		mod_asset->rot_preview.previous = glm::vec2();
		mod_asset->rot_preview.rot = glm::quat();
	}

	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Materials");

	for (int i = 0; i < mod_asset->materials.length; i++) {
		std::string prefix = model->materials[i] + " : ";
		Material_inspect(&mod_asset->materials[i], reflect::TypeResolver<Material>::get(), prefix, world);
	}

	rot_preview(mod_asset->rot_preview);

	render_preview_for(world, self, *mod_asset, params);
}

std::string name_from_filename(std::string& filename) {
	int after_slash = filename.find_last_of("\\") + 1;
	return filename.substr(after_slash, filename.find_last_of(".") - after_slash);
}

void add_asset(AssetTab& self, ID id) {
	self.assets.by_id<AssetFolder>(self.current_folder)->contents.append(id);
}

void import_model(World& world, Editor& editor, AssetTab& self, RenderParams& params, std::string& filename) {	
	Handle<Model> handle = load_Model(filename);
	Model* model = RHI::model_manager.get(handle);

	ID id = self.assets.make_ID();
	ModelAsset* model_asset = self.assets.make<ModelAsset>(id);
	model_asset->handle = handle;
	for (std::string& name : model->materials) {
		model_asset->materials.append(self.default_material);
	}

	model_asset->name = name_from_filename(filename);
	
	render_preview_for(world, self, *model_asset, params);

	add_asset(self, id);
}

void import_texture(Editor& editor, AssetTab& self, std::string& filename) {
	Handle<Texture> handle = load_Texture(filename);

	ID id = self.assets.make_ID();
	TextureAsset* folder = self.assets.make<TextureAsset>(id);
	folder->handle = handle;
	folder->name = name_from_filename(filename);

	add_asset(self, id);
}

void import_shader(Editor& editor, AssetTab& self, std::string& filename) {
	std::string frag_filename = filename.substr(0, filename.find(".vert"));
	frag_filename += ".frag";

	Handle<Shader> handle = load_Shader(filename, frag_filename);
	ID id = self.assets.make_ID();
	ShaderAsset* shader_asset = self.assets.make<ShaderAsset>(id);

	add_asset(self, id);
}

void import_filename(Editor& editor, World& world, RenderParams& params, AssetTab& self, std::wstring& w_filename) {
	std::string filename = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(w_filename);
		
	std::string asset_path;
	if (!Level::to_asset_path(filename, &asset_path)) {
		asset_path = filename.substr(filename.find_last_of("\\") + 1, filename.size());

		std::string new_filename = Level::asset_path(asset_path);
		std::wstring w_new_filename = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(new_filename);

		if (!CopyFile(w_filename.c_str(), w_new_filename.c_str(), true)) {
			log("Could not copy filename");
		}
	}

	log(asset_path);

	if (endsWith(asset_path, ".fbx")) {
		import_model(world, editor, self, params, asset_path);
	}
	else if (endsWith(asset_path, ".jpg") || endsWith(asset_path, ".png")) {
		import_texture(editor, self, asset_path);
	}
	else if (endsWith(asset_path, ".vert") || endsWith(asset_path, ".frag")) {
		import_shader(editor, self, asset_path);
	}
	else {
		log("Unsupported extension for", asset_path);
	}
}

vector<MaterialAsset*> AssetTab::material_handle_to_asset;

MaterialAsset* create_new_material(World& world, AssetTab& self, Editor& editor, RenderParams& params) {
	Handle<Material> mat_handle = create_default_material();

	ID id = self.assets.make_ID();
	MaterialAsset* asset = self.assets.make<MaterialAsset>(id);
	asset->handle = mat_handle;
	asset->name = "Empty";
	
	render_preview_for(world, self, *asset, params);
	add_asset(self, id);

	int diff = asset->handle.id - self.material_handle_to_asset.length + 1;

	for (int i = 0; i < diff; i++) {
		self.material_handle_to_asset.append(NULL);
	}
	self.material_handle_to_asset[asset->handle.id] = asset;

	return asset;
}

AssetTab::AssetTab() {
	{
		AttachmentSettings attachment(this->preview_map);

		FramebufferSettings settings;
		settings.width = 512;
		settings.height = 512;
		settings.color_attachments.append(attachment);

		this->preview_fbo = Framebuffer(settings);
	}

	{
		AttachmentSettings attachment(this->preview_tonemapped_map);

		FramebufferSettings settings;
		settings.width = 512;
		settings.height = 512;
		settings.color_attachments.append(attachment);

		this->preview_tonemapped_fbo = Framebuffer(settings);
	}

	assets.add(new Store<AssetFolder>(100));
	assets.add(new Store<ModelAsset>(100));
	assets.add(new Store<TextureAsset>(100));
	assets.add(new Store<MaterialAsset>(100));
	assets.add(new Store<ShaderAsset>(100));

	ID id = assets.make_ID();

	AssetFolder* folder = assets.make<AssetFolder>(id);
	folder->name = "Assets";
	
	this->toplevel = id;
	this->current_folder = id;
}

void AssetTab::update(World& world, Editor& editor, UpdateParams& params) {
	if (params.input.key_pressed(GLFW_KEY_N)) {
		if (selected != -1) {
			ModelAsset* model_asset = assets.by_id<ModelAsset>(selected);

			if (model_asset) {

				ID id = world.make_ID();
				Entity* e = world.make<Entity>(id);

				EntityEditor* name = world.make<EntityEditor>(id);
				name->name = model_asset->name;

				Transform * trans = world.make<Transform>(id);

				Materials* materials = world.make<Materials>(id);
				materials->materials = model_asset->materials;

				ModelRenderer* mr = world.make<ModelRenderer>(id);
				mr->model_id = model_asset->handle;

				Camera* cam = get_camera(world, params.layermask);
				Transform* cam_trans = world.by_id<Transform>(world.id_of(cam));
				trans->position = cam_trans->position + (cam_trans->rotation * glm::vec3(0, 0, -10));

			}
		}

	}
}

void AssetTab::render(World& world, Editor& editor, RenderParams& params) {
	if (ImGui::Begin("Assets")) {
		if (ImGui::BeginPopup("CreateAsset"))
		{
			if (ImGui::MenuItem("Import")) {
				std::wstring filename = open_dialog(editor);
				if (filename != L"") import_filename(editor, world, params, *this, filename);
			}
			
			if (ImGui::MenuItem("New Material"))
			{
				create_new_material(world, *this, editor, params);
			}

			if (ImGui::MenuItem("New Folder"))
			{
				ID id = assets.make_ID();
				AssetFolder* folder = assets.make<AssetFolder>(id);
				folder->name = "Empty Folder";
				folder->owner = this->current_folder;

				add_asset(*this, id);
			}

			ImGui::EndPopup();
		}

		{
			DefferedMoveFolder deffered_move_folder;

			int folder_tree = this->current_folder;
			vector<const char*> folder_file_path;
			vector<ID> folder_id;
			folder_file_path.allocator = &temporary_allocator;
			folder_id.allocator = &temporary_allocator;

			while (folder_tree >= 0) {
				AssetFolder* folder = assets.by_id<AssetFolder>(folder_tree);
				folder_tree = folder->owner;

				folder_file_path.append(folder->name.c_str());
				folder_id.append(assets.id_of(folder));
			}

			for (int i = folder_file_path.length - 1; i >= 0; i--) {
				if (ImGui::Button(folder_file_path[i])) {
					this->current_folder = folder_id[i];
				}

				deffered_move_folder.accept_drop(folder_id[i]);

				ImGui::SameLine();
				ImGui::Text("\\");
				ImGui::SameLine();
			}

			deffered_move_folder.apply(*this);
		}

		{
			char buff[50];
			memcpy(buff, filter.c_str(), filter.size() + 1);
			ImGui::SameLine(ImGui::GetWindowWidth() - 400);
			ImGui::Text("Filter");
			ImGui::SameLine();
			ImGui::InputText("Filter", buff, 50);
			filter = std::string(buff);
		}

		ImGui::Separator();

		render_assets(editor, *this, filter);

		if (ImGui::IsWindowHovered()) {
			//if (ImGui::GetIO().MouseClicked[0] && ImGui::IsAnyItemActive()) selected = -1;
			if (ImGui::GetIO().MouseClicked[1]) ImGui::OpenPopup("CreateAsset");
		}
	}

	ImGui::End();

	if (ImGui::Begin("Asset Settings")) {
		if (selected != -1) {
			auto tex = assets.by_id<TextureAsset>(selected);
			auto shad = assets.by_id<ShaderAsset>(selected);
			auto mod = assets.by_id<ModelAsset>(selected);
			auto mat = assets.by_id<MaterialAsset>(selected);

			if (tex) asset_properties(tex, editor, world, *this, params);
			if (shad) asset_properties(shad, editor, world, *this, params);
			if (mod) asset_properties(mod, editor, world, *this, params);
			if (mat) asset_properties(mat, editor, world, *this, params);
		}
	}

	ImGui::End();
}