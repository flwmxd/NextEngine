#include "stdafx.h"
#include "editor/assetTab.h"
#include <imgui.h>
#include "ecs/ecs.h"
#include "editor/editor.h"
#include "graphics/rhi.h"
#include "imgui/imgui_internal.h"
#include "components/camera.h"
#include "logger/logger.h"
#include "core/vfs.h"
#include "core/input.h"

float GRID_SZ = 64.0f;

glm::vec2 to_vec2(ImVec2 vec) {
	return glm::vec2(vec.x, vec.y);
}

ImVec2 from_vec2(glm::vec2 vec) {
	return ImVec2(vec.x, vec.y);
}

ShaderNode::ShaderNode(Type type, ChannelType output_type) {
	this->output.type = output_type;
	this->type = type;
}

ShaderEditor::ShaderEditor() {}

ShaderNode::InputChannel::InputChannel(StringView name, ShaderNode::ChannelType type) {
	memset(this, 0, sizeof(InputChannel));

	this->type = type;
	this->name = name;
}

ShaderNode::InputChannel::InputChannel(StringView name, glm::vec4 value) : InputChannel(name, ShaderNode::Channel4) {
	this->value4 = value;
}

ShaderNode::InputChannel::InputChannel(StringView name, glm::vec3 value) : InputChannel(name, ShaderNode::Channel3) {
	this->value3 = value;
}

ShaderNode::InputChannel::InputChannel(StringView name, glm::vec2 value) : InputChannel(name, ShaderNode::Channel2) {
	this->value2 = value;
}

ShaderNode::InputChannel::InputChannel(StringView name, float value) : InputChannel(name, ShaderNode::Channel1) {
	this->value1 = value;
}

glm::vec2 calc_screen_space(ShaderGraph& graph,  glm::vec2 position = {}) {
	position.x += ImGui::GetWindowPos().x + graph.scrolling.x;
	position.y += ImGui::GetWindowPos().y + 50.0f + graph.scrolling.y;
	return graph.scale * position;
}


float calc_extra_height(ShaderGraph& graph, ShaderNode* node) {
	if (node->type == ShaderNode::TEXTURE_NODE) return graph.scale * 380 + 10.0f;
	if (node->type == ShaderNode::PBR_NODE) return graph.scale * 380 + 10.0f;
	return 0;
}

float override_width_of_node(ShaderNode* self);

glm::vec2 calc_size(ShaderGraph& graph, ShaderNode* self) {
	float width = override_width_of_node(self);
	return  (self->inputs.length * 2.5f) + graph.scale * glm::vec2(width, 70.0f + fmax(1.0f, self->inputs.length) * 40.f) + glm::vec2(0, calc_extra_height(graph, self));
}

bool mouse_hovering(glm::vec2 position, glm::vec2 size) {
	ImVec2 prev = ImGui::GetCurrentWindow()->DC.CursorPos;
	
	ImGui::SetCursorScreenPos(ImVec2(position.x, position.y));
	ImGui::InvisibleButton("node", ImVec2(size.x, size.y));

	bool hovered = ImGui::IsItemHovered();

	ImGui::SetCursorScreenPos(prev);

	return hovered;
}

bool mouse_hovering_node(ShaderGraph& graph, ShaderNode* self) {
	return mouse_hovering(calc_screen_space(graph, self->position), calc_size(graph, self));
}

template<typename T>
ShaderNode::Link* make_link(T& channel, Handle<ShaderNode>) {}

template<>
ShaderNode::Link* make_link(ShaderNode::InputChannel& channel, Handle<ShaderNode> node) {
	channel.link.to = node;
	channel.link.from.id = { INVALID_HANDLE };
	return &channel.link;
}

template<>
ShaderNode::Link* make_link(ShaderNode::OutputChannel& channel, Handle<ShaderNode> node) {
	channel.links.append(ShaderNode::Link());
	ShaderNode::Link* link = &channel.links[channel.links.length - 1];
	link->to.id = { INVALID_HANDLE };
	link->from = node;
	return link;
}

template<typename T>
void connect_link(ShaderGraph& graph, T& channel, Handle<ShaderNode>, unsigned int index) {}

template<>
void connect_link(ShaderGraph& graph, ShaderNode::InputChannel& channel, Handle<ShaderNode> node, unsigned int index) {
	ShaderNode::Link* link = graph.action.link;

	if (!link->to.id || !link->from.id) return;
	
	link->to = node;
	link->index = index;

	//Links should be owned by input channel
	channel.link = *link;

	//Remove link from result node
	ShaderNode* node_ptr = graph.nodes_manager.get(link->from);

	for (unsigned int i = 0; i < node_ptr->output.links.length; i++) {
		if (&node_ptr->output.links[i] == link) {
			ShaderNode::Link popped_link = node_ptr->output.links.pop();

			if (node_ptr->output.links.length > 0) node_ptr->output.links[i] = popped_link;
			break;
		}
	}
}

template<typename T>
void connect_temporary_link(ShaderGraph& graph, T& channel, Handle<ShaderNode>, unsigned int index) {}

template<>
void connect_temporary_link(ShaderGraph& graph, ShaderNode::OutputChannel& channel, Handle<ShaderNode> node, unsigned int index) {
	ShaderNode::Link* link = graph.action.link;
	
	if (link->from.id) return; //already is an output
	if (node.id == graph.action.link->to.id) return; //Trying to connect same nodes input to its output
	
	link->from = node;
}


template<>
void connect_temporary_link(ShaderGraph& graph, ShaderNode::InputChannel& channel, Handle<ShaderNode> node, unsigned int index) {
	ShaderNode::Link* link = graph.action.link;
	
	if (link->to.id) return; //already is an input
	if(node.id == graph.action.link->from.id) return; //Trying to connect same nodes input to its output
	
	link->to = node;
	link->index = index;
}

template<typename T>
bool is_input(T&) {};

template<>
bool is_input(ShaderNode::InputChannel& channel) { return true; }

template<>
bool is_input(ShaderNode::OutputChannel& channel) { return false; }

template<typename T>
void draw_connector(ShaderGraph& graph, glm::vec2 start_position, Handle<ShaderNode> node, T& connector, unsigned int index = 0) {
	auto window = ImGui::GetCurrentWindow();
	auto draw_list = window->DrawList;

	connector.position = start_position;
	
	draw_list->AddCircleFilled(ImVec2(start_position.x, start_position.y), 10.0f * graph.scale, ImColor(120, 120, 120));

	glm::vec2 begin_hitbox = glm::vec2(start_position.x - graph.scale * (is_input(connector) ? 30.0f : 20.0f), start_position.y - graph.scale * 20.0f);
	glm::vec2 hitbox_size = glm::vec2(graph.scale * 40.0f);

	if (graph.action.type == ShaderGraph::Action::None && ImGui::IsMouseDragging() && mouse_hovering(begin_hitbox, hitbox_size)) {
		graph.action.type = ShaderGraph::Action::Dragging_Connector;
		graph.action.link = make_link(connector, node);
		graph.action.link->index = index;
	}

	if (graph.action.type == ShaderGraph::Action::Dragging_Connector && mouse_hovering(begin_hitbox, hitbox_size)) {
		connect_temporary_link(graph, connector, node, index);
	}

	if (graph.action.type == ShaderGraph::Action::Releasing_Connector && mouse_hovering(begin_hitbox, hitbox_size)) {
		connect_link(graph, connector, node, index);
		graph.action.type = ShaderGraph::Action::None;
	}

	//draw_list->AddRectFilled(ImVec2(begin_hitbox.x, begin_hitbox.y), ImVec2(begin_hitbox.x + hitbox_size.x, begin_hitbox.y + hitbox_size.y), ImColor(255, 255, 255));
}

glm::vec2 calc_pos_of_node(ShaderGraph& graph, Handle<ShaderNode> handle, bool is_selected);

glm::vec2 calc_pos_of_output(ShaderGraph& graph, Handle<ShaderNode> handle) {
	return graph.nodes_manager.get(handle)->output.position;
}

glm::vec2 calc_pos_of_input(ShaderGraph& graph, Handle<ShaderNode> handle, unsigned int index) {
	return graph.nodes_manager.get(handle)->inputs[index].position;
}

ImVec2 calc_link_pos(ShaderGraph& graph, ShaderNode::Link* link) {
	glm::vec2 result;
	if (link->from.id) result = calc_pos_of_output(graph, link->from);
	if (link->to.id) result = calc_pos_of_input(graph, link->to, link->index);

	return ImVec2(result.x, result.y);
}

void render_bezier_link(ShaderGraph& graph, ImVec2 start, ImVec2 end, bool not_connected = false) {
	glm::vec2 v_start(start.x, start.y);
	glm::vec2 v_end(end.x, end.y);

	float bezier = 200.0f;

	float length = glm::length(v_start.x - v_end.x);

	length = glm::min(length * 0.5f, 50.0f);

	glm::vec2 cp0, cp1;

	if (not_connected && start.x > end.x) {
		std::swap(v_start, v_end);
		std::swap(start, end);
	}
	
	if (v_start.x > v_end.x && false) {
		cp0 = v_start - glm::vec2(length, 0);
		cp1 = v_end + glm::vec2(length, 0);
	}
	else {
		cp0 = v_start + glm::vec2(length, 0);
		cp1 = v_end - glm::vec2(length, 0);
	}

	ImGui::GetCurrentWindow()->DrawList->AddLine(start, ImVec2(cp0.x, cp0.y), ImColor(120, 120, 120), graph.scale * 5.f);
	ImGui::GetCurrentWindow()->DrawList->AddLine( ImVec2(cp0.x, cp0.y), ImVec2(cp1.x, cp1.y), ImColor(120, 120, 120), graph.scale * 5.f);
	ImGui::GetCurrentWindow()->DrawList->AddLine( ImVec2(cp1.x, cp1.y), end, ImColor(120, 120, 120), graph.scale * 5.f);
}


void render_complete_link(ShaderGraph& graph, ShaderNode::Link* link) {
	ShaderNode* node_result = graph.nodes_manager.get(link->from);
	ShaderNode* node_input = graph.nodes_manager.get(link->to);

	if (node_result && node_input) {
		glm::vec2 pos1 = calc_pos_of_output(graph, link->from); 
		glm::vec2 pos2 = calc_pos_of_input(graph, link->to, link->index);

		render_bezier_link(graph, from_vec2(pos1), from_vec2(pos2));
	}
}

void render_link(ShaderGraph& graph, ShaderNode::Link* link, bool is_input) {
	
	if (graph.action.type == ShaderGraph::Action::Dragging_Connector && graph.action.link == link) {
		if (ImGui::IsMouseDragging()) {
			ShaderNode* node_result = graph.nodes_manager.get(link->from);
			ShaderNode* node_input = graph.nodes_manager.get(link->to);

			if (node_result && node_input) {
				render_complete_link(graph, link);
				if (is_input) link->from.id = INVALID_HANDLE;
				else link->to.id = INVALID_HANDLE;
			}
			else {
				render_bezier_link(graph, calc_link_pos(graph, link), ImGui::GetMousePos(), true);
			}
		}
		else {
			graph.action.type = ShaderGraph::Action::Releasing_Connector;
		}
	}
	else if (graph.action.type == ShaderGraph::Action::Releasing_Connector && graph.action.link == link) {
		graph.action.type = ShaderGraph::Action::None;
	}
	else {
		render_complete_link(graph, link);
	}
}

void render_input(ShaderGraph& graph, Handle<ShaderNode> handle, unsigned int i, bool render_default) {
	ShaderNode* node = graph.nodes_manager.get(handle);
	ShaderNode::InputChannel& input = node->inputs[i];
	
	auto window = ImGui::GetCurrentWindow();
	ImGui::SetNextItemWidth(200 * graph.scale);

	glm::vec2 start_position = glm::vec2(window->DC.CursorPos.x - 10.0f * graph.scale, window->DC.CursorPos.y + 20.0f * graph.scale);

	draw_connector(graph, start_position, handle, input, i);

	bool is_connected = input.link.from.id != INVALID_HANDLE;

	ImGui::PushID(handle.id);

	if (input.type == ShaderNode::Channel1 && !is_connected && render_default) { 
		ImGui::Dummy(ImVec2(0, 40.0f * graph.scale));
		ImGui::SameLine();
		ImGui::InputFloat(input.name.c_str(), &input.value1); 
	}
	else if (input.type == ShaderNode::Channel2 && !is_connected && render_default) ImGui::InputFloat2(input.name.c_str(), &input.value2.x);
	else if (input.type == ShaderNode::Channel3 && !is_connected && render_default) {
		edit_color(input.value3, input.name, ImVec2(40 * graph.scale, 40 * graph.scale));
		ImGui::SameLine();
		ImGui::Text(input.name.c_str());
	}
	else if (input.type == ShaderNode::Channel4 && !is_connected && render_default) {
		edit_color(input.value4, input.name, ImVec2(40 * graph.scale, 40 * graph.scale));
		ImGui::SameLine();
		ImGui::Text(input.name.c_str());
	}
	else {
		ImGui::Dummy(ImVec2(0, 40 * graph.scale));
		ImGui::SameLine();
		ImGui::Text(input.name.c_str());
		
	}

	ImGui::PopID();
}

void render_output(ShaderGraph& graph, Handle<ShaderNode> handle) {
	ShaderNode* node = graph.nodes_manager.get(handle);
	ShaderNode::OutputChannel& output = node->output;
	float width = override_width_of_node(node);

	auto window = ImGui::GetCurrentWindow();

	if (output.type != ShaderNode::ChannelNone) {
		glm::vec2 start_position = glm::vec2(window->DC.CursorPos.x + (width - 10.0f) * graph.scale, window->DC.CursorPos.y + 20.0f * graph.scale);
		draw_connector(graph, start_position, handle, output);
	}
}

glm::vec2 snap_to_grid(ShaderGraph& graph, glm::vec2 position) {
	return glm::vec2(glm::round(position.x / GRID_SZ) * GRID_SZ, glm::round(position.y / GRID_SZ) * GRID_SZ);
}

glm::vec2 screenspace_to_position(ShaderGraph& graph, glm::vec2 position) {
	position /= graph.scale;

	position.x -= ImGui::GetWindowPos().x + graph.scrolling.x;
	position.y -= ImGui::GetWindowPos().y + graph.scrolling.y + 50.0f;

	return position;
}


glm::vec2 calc_pos_of_node(ShaderGraph& graph, Handle<ShaderNode> handle, bool is_selected) {
	auto self = graph.nodes_manager.get(handle);

	glm::vec2 position = calc_screen_space(graph, self->position);
	bool is_snapping = ImGui::IsKeyDown(GLFW_KEY_LEFT_CONTROL);
	
	//If Dragging this node
	if ((graph.action.type == ShaderGraph::Action::Moving_Node && is_selected) || (graph.action.type == ShaderGraph::Action::Dragging_Node && graph.action.drag_node.id == handle.id)) {

		bool is_done = !ImGui::IsMouseDragging();
		glm::vec2 delta = glm::vec2(ImGui::GetMouseDragDelta().x, ImGui::GetMouseDragDelta().y);

		if (graph.action.type == ShaderGraph::Action::Moving_Node) { //Solution to this problem, is to create a new action to apply the movement
			is_done = ImGui::IsMouseClicked(0);
			delta = glm::vec2(ImGui::GetMousePos().x - graph.action.mouse_pos.x, ImGui::GetMousePos().y - graph.action.mouse_pos.y);
		}
		
		if (!is_done) {
			position += delta;
			if (is_snapping) position =  calc_screen_space(graph, snap_to_grid(graph, screenspace_to_position(graph, position)));
		}
		else {
			graph.action.type = ShaderGraph::Action::ApplyMove;
			graph.action.mouse_pos = delta;
		}
	}

	if (graph.action.type == ShaderGraph::Action::ApplyMove && is_selected) {
		self->position += graph.action.mouse_pos / graph.scale;

		if (is_snapping) self->position = snap_to_grid(graph, self->position); //LEFT OF HERE

		position = calc_screen_space(graph, self->position);
	}

	return position;
}

void render_node_inner(ShaderGraph& graph, ShaderNode* self, Handle<ShaderNode> handle);

void deselect(ShaderGraph& graph, Handle<ShaderNode> handle) {
	for (auto& selected_handle : graph.selected) {
		if (selected_handle.id == handle.id) {
			selected_handle = graph.selected.pop();
			break;
		}
	}
}

void render_node(ShaderGraph& graph, Handle<ShaderNode> handle) {
	auto self = graph.nodes_manager.get(handle);

	glm::vec2 size = calc_size(graph, self);

	auto window = ImGui::GetCurrentWindow();
	
	auto draw_list = ImGui::GetWindowDrawList();

	bool is_selected = false;
	for (auto selected_handle : graph.selected) { //todo should add equals operator to handle to avoid this
		if (selected_handle.id == handle.id) {
			is_selected = true;
			break;
		}
	}

	if (graph.action.type == ShaderGraph::Action::BoxSelectApply) {
		glm::vec2 start = graph.action.mouse_pos;
		glm::vec2 end = start + graph.action.mouse_delta;
		
		if (start.x > end.x) std::swap(start.x, end.x);
		if (start.y > end.y) std::swap(start.y, end.y);
		
		glm::vec2 position = calc_screen_space(graph, self->position);
		
		if (position.x > start.x && position.y > start.y && position.x + size.x < end.x && position.y + size.y < end.y) { //In Box Selection
			if (ImGui::IsKeyDown(GLFW_KEY_LEFT_SHIFT)) {
				deselect(graph, handle);
				is_selected = false;
			}
			else if (!is_selected) {
				is_selected = true;
				graph.selected.append(handle);
			}
		}

		
	}

	//Begin move
	if (ImGui::IsKeyDown(GLFW_KEY_G)) {
		graph.action.type = ShaderGraph::Action::Moving_Node;
		graph.action.mouse_pos = glm::vec2(ImGui::GetMousePos().x, ImGui::GetMousePos().y);
	}

	glm::vec2 position = calc_pos_of_node(graph, handle, is_selected);

	if (is_selected) {
		draw_list->ChannelsSetCurrent(2);

		float frame_border = 1.0f;
		draw_list->AddRect(ImVec2(position.x - frame_border, position.y - frame_border), ImVec2(position.x + size.x + frame_border, position.y + size.y + frame_border), ImColor(255, 150, 0), 5.0f);
	}

	draw_list->AddRectFilled(ImVec2(position.x, position.y), ImVec2(position.x + size.x, position.y + size.y), ImColor(80, 80, 80), 5.0f);
	draw_list->AddRectFilled(ImVec2(position.x, position.y), ImVec2(position.x + size.x, position.y + 50.0f * graph.scale), ImColor(40, 40, 40), 5.0f);
	draw_list->AddRectFilled(ImVec2(position.x, position.y + 45.0f * graph.scale), ImVec2(position.x + size.x, position.y + 50.0f * graph.scale), ImColor(40, 40, 40));

	float padding = graph.scale * 10.0f;

	ImGui::SetCursorScreenPos(ImVec2(position.x + padding, position.y + padding));
	
	//Draw Node Window
	ImGui::BeginGroup(); // Lock horizontal position
	//ImGui::PopID();

	render_node_inner(graph, self, handle);

	for (int i = 0; i < self->inputs.length; i++) {
		auto& input = self->inputs[i];
	}

	ImGui::EndGroup();

	//Select or unselect
	if (ImGui::IsMouseClicked(0) && mouse_hovering_node(graph, self) && graph.action.type == ShaderGraph::Action::None) {
		if (ImGui::IsKeyDown(GLFW_KEY_LEFT_SHIFT)) {
			if (is_selected) {
				is_selected = false;
				deselect(graph, handle);
			}
			else {
				graph.selected.append(handle);
			}
		}
		else {
			graph.selected.clear();
			graph.selected.append(handle);
			is_selected = true;
		}
	}

	//Begin Drag
	if (graph.action.type == ShaderGraph::Action::None && ImGui::IsMouseDragging() && mouse_hovering_node(graph, self) && is_selected) {
		graph.action.type = ShaderGraph::Action::Dragging_Node;
		graph.action.drag_node = handle;
	}

	draw_list->ChannelsSetCurrent(1);
}

#include "GLFW/glfw3.h"

void set_params_for_shader_graph(Material* mat_ptr) {
	mat_ptr->params.clear();

	ShaderAsset* asset = AssetTab::shader_handle_to_asset[mat_ptr->shader.id];

	if (asset->graph->requires_time) {
		Param p;
		p.type = Param_Time;
		p.loc = location(mat_ptr->shader, "_dep_time");

		mat_ptr->params.append(p);
	}

	for (Param p : asset->graph->dependencies) {
		p.loc = location(mat_ptr->shader, get_dependency({ p.loc.id }));
		mat_ptr->params.append(p);
	}

	mat_ptr->set_vec2("transformUVs", glm::vec2(1.0f));
}

void render_preview(ShaderAsset* asset, World& world, Editor& editor, RenderParams& old_params) {
	ShaderGraph& graph = *asset->graph;
	
	ID id = world.make_ID();
	Entity* entity = world.make<Entity>(id);
	Transform* trans = world.make<Transform>(id);
	trans->position.z = 2.5;
	Camera* cam = world.make<Camera>(id);

	Model* model = RHI::model_manager.get(load_Model("sphere.fbx"));

	Transform trans_of_sphere;
	trans_of_sphere.rotation = graph.rot_preview.rot;

	glm::mat4 model_m = trans_of_sphere.compute_model_matrix();
	
	Handle<Material> mat_handle;
	if (asset->handle.id == INVALID_HANDLE) mat_handle = editor.asset_tab.default_material;
	else {
		static Handle<Material> preview_handle = RHI::material_manager.make({});
		Material* mat_ptr = RHI::material_manager.get(preview_handle);
		*mat_ptr = Material(asset->handle);
		set_params_for_shader_graph(mat_ptr);

		mat_handle = preview_handle;
	}

	vector<Handle<Material>> materials = { mat_handle };

	CommandBuffer cmd_buffer;
	RenderParams render_params = create_preview_command_buffer(cmd_buffer, old_params, editor.asset_tab, cam, world);

	model->render(0, &model_m, materials, render_params);

	render_preview_to_buffer(editor.asset_tab, render_params, cmd_buffer, graph.rot_preview.preview, world);

	world.free_now_by_id(id);
}

glm::vec2 shift_by_grid(ShaderGraph& graph, glm::vec2 pos, glm::vec2 shift) {
	return calc_screen_space(graph, screenspace_to_position(graph, pos) + shift);
}

void draw_grid(ShaderGraph& graph, float grid_sz, float width) {
	auto draw_list = ImGui::GetCurrentWindow()->DrawList;

	//Draw Grid
	ImU32 GRID_COLOR = IM_COL32(200, 200, 200, 40);
	ImVec2 win_pos = ImGui::GetWindowPos();
	win_pos.y += 50.0f;
	ImVec2 canvas_sz = ImGui::GetWindowSize();
	
	GRID_SZ = grid_sz; //todo turn into paremeter
	glm::vec2 start = calc_screen_space(graph, snap_to_grid(graph, screenspace_to_position(graph, glm::vec2(0, 0))));

	for (glm::vec2 pos = start; pos.x < canvas_sz.x + win_pos.x;) {
		draw_list->AddLine(ImVec2(pos.x, win_pos.y), ImVec2(pos.x, canvas_sz.y + win_pos.y), GRID_COLOR, width);
		pos = shift_by_grid(graph, pos, glm::vec2(GRID_SZ, 0));
	}


	for (glm::vec2 pos = start; pos.y < canvas_sz.y + win_pos.y;) {
		glm::vec2 rel_pos = screenspace_to_position(graph, pos);
		draw_list->AddLine(ImVec2(win_pos.x, pos.y), ImVec2(canvas_sz.x + win_pos.x, pos.y), GRID_COLOR, width);
		pos = shift_by_grid(graph, pos, glm::vec2(0, GRID_SZ));
	}

	GRID_SZ = 64.0f;
}

void set_scale(ShaderGraph* graph, Input& input, float scale) {
	scale = glm::clamp(scale, 0.1f, 2.0f);

	glm::vec2 mouse_pos(ImGui::GetMousePos().x, ImGui::GetMousePos().y);
	glm::vec2 mouse_rel = screenspace_to_position(*graph, mouse_pos);

	if (scale != graph->scale) {		
		graph->scale = scale;
		graph->scrolling += screenspace_to_position(*graph, mouse_pos) - mouse_rel; //Zoom in a point
	}
}

void ShaderGraph::render(World& world, Editor& editor, RenderParams& params, Input& input) {
	ImGui::PushClipRect(ImGui::GetCursorScreenPos(), ImVec2(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionMax().x, ImGui::GetCursorScreenPos().y + ImGui::GetContentRegionMax().y), true);

	float scroll_speed = 0.2f;

	if (input.scroll_offset > 0) {
		set_scale(this, input, scale * (1.0 + input.scroll_offset * scroll_speed));
	}
	else if (input.scroll_offset < 0) {
		set_scale(this, input, scale  / (1.0 - input.scroll_offset * scroll_speed));
	}
	
	float target_font_size = 32.0f * scale;

	auto g = ImGui::GetCurrentContext();
	float fontBaseSize = g->FontBaseSize;
	float fontSize = g->FontSize;

	g->FontSize = target_font_size;
	g->FontBaseSize = (g->FontSize / fontSize) * g->FontBaseSize;

	draw_grid(*this, 64.0f, 1.0f);
	draw_grid(*this, 64.0f * 5.0f, 3.0f);

	auto draw_list = ImGui::GetCurrentWindow()->DrawList;

	draw_list->ChannelsSplit(4);

	draw_list->ChannelsSetCurrent(1);

	//Draw Nodes
	for (unsigned int i = 0; i < nodes_manager.slots.length; i++) {
		render_node(*this, nodes_manager.index_to_handle(i));
	}

	draw_list->ChannelsSetCurrent(0);

	//Draw Links
	for (unsigned int i = 0; i < nodes_manager.slots.length; i++) {
		ShaderNode* node = nodes_manager.get(nodes_manager.index_to_handle(i));

		for (auto& input : node->inputs) {
			render_link(*this, &input.link, true);
		}

		for (auto& link : node->output.links) {
			render_link(*this, &link, false);
		}
	}

	draw_list->ChannelsSetCurrent(3);

	if (action.type == Action::BoxSelect) {
		glm::vec2 start = action.mouse_pos;
		glm::vec2 end = start;
		end.x += ImGui::GetMouseDragDelta().x;
		end.y += ImGui::GetMouseDragDelta().y;

		float length = 5.0f;

		auto line_color = ImColor(200, 200, 200);
		float line_width = 2.0f;

		for (float x = start.x; x < end.x - length; x += length * 2) draw_list->AddLine(ImVec2(x, start.y), ImVec2(x + length, start.y), line_color, line_width);
		for (float x = start.x; x < end.x - length; x += length * 2) draw_list->AddLine(ImVec2(x, end.y), ImVec2(x + length, end.y), line_color, line_width);
		for (float y = start.y; y < end.y - length; y += length * 2) draw_list->AddLine(ImVec2(start.x, y), ImVec2(start.x, y + length), line_color, line_width);
		for (float y = start.y; y < end.y - length; y += length * 2) draw_list->AddLine(ImVec2(end.x, y), ImVec2(end.x, y + length), line_color, line_width);

		draw_list->AddRectFilled(ImVec2(start.x, start.y), ImVec2(end.x, end.y), ImColor(40.0f, 40.0f, 40.0f, 0.2f));
	}


	draw_list->ChannelsMerge();

	// Scrolling
	if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive() && ImGui::IsMouseDragging(2, 0.0f)) {
		scrolling += glm::vec2(ImGui::GetIO().MouseDelta.x, ImGui::GetIO().MouseDelta.y) / scale;
	}

	g->FontBaseSize = fontBaseSize;
	g->FontSize = fontSize;

	ImGui::PopClipRect();
}

StringBuffer get_dependency(Handle<ShaderNode> node) {
	return format("_dep", node.id);
}

ShaderCompiler::ShaderCompiler(ShaderGraph& graph) : graph(graph) {

}

void ShaderCompiler::begin() {
	contents +=
		"out vec4 FragColor;\n"
		"in vec2 TexCoords;\n"
		"in vec3 Normal;\n"
		"in vec3 FragPos;\n"
		"in mat3 TBN;\n\n"

		"#include shaders/pbr_helper.glsl\n\n"
		;

	for (Param& param : graph.dependencies) {
		if (param.type == Param_Image) {
			contents += "uniform sampler2D ";
			contents += get_dependency({ param.loc.id });
			contents += ";\n";
		}
	}

	if (graph.requires_time) {
		contents += "uniform float _dep_time;\n";
	}

	contents += 
		"void main()\n"
		"{\n"
		;
}

void ShaderCompiler::end() {
	contents +=
		"#endif\n"
		"}\n";
}

void ShaderCompiler::compile_input(ShaderNode::InputChannel& input) {
	if (input.link.from.id == INVALID_HANDLE) {
		if (input.type == ShaderNode::Channel1) {
			contents += format(input.value1);
		}

		if (input.type == ShaderNode::Channel2) {
			contents += format("vec2(", input.value2.x, ",", input.value2.y, ")");
		}

		if (input.type == ShaderNode::Channel3) {
			contents += format("vec3(", input.value3.x, ",", input.value3.y, ",", input.value3.z, ")");
		}

		if (input.type == ShaderNode::Channel4) {
			contents += format("vec4(", input.value3.x, ",", input.value3.y, ",", input.value3.z, ",", input.value4.w, ")");
		}
	}
	else {
		ShaderNode* output = graph.nodes_manager.get(input.link.from);

		if (output->output.type > input.type) {
			compile_node(input.link.from);
				
			if (input.type == ShaderNode::Channel1) contents += ".x";
			if (input.type == ShaderNode::Channel2) contents += ".xy";
			if (input.type == ShaderNode::Channel3) contents += ".xyz";
		}
		else if (output->output.type < input.type) {
			if (input.type == ShaderNode::Channel4) {
				if (output->output.type == ShaderNode::Channel1) {
					contents += "vec4(vec3(";
					compile_node(input.link.from);
					contents += "), 1.0f)";
				}

				if (output->output.type == ShaderNode::Channel2) {
					contents += "vec4(";
					compile_node(input.link.from);
					contents += "0.0f, 1.0f)";
				}

				if (output->output.type == ShaderNode::Channel3) {
					contents += "vec4(";
					compile_node(input.link.from);
					contents += "1.0f)";
				}
			}
				
			if (input.type == ShaderNode::Channel3) {
				if (output->output.type == ShaderNode::Channel1) {
					contents += "vec3(";
					compile_node(input.link.from);
					contents += ")";
				}

				if (output->output.type == ShaderNode::Channel2) {
					contents += "vec3(";
					compile_node(input.link.from);
					contents += "0.0f)";
				}
			}

			if (input.type == ShaderNode::Channel2) {
				if (output->output.type == ShaderNode::Channel1) {
					contents += "vec2(";
					compile_node(input.link.from);
					contents += ")";
				}
			}
		}
		else {
			compile_node(input.link.from);
		}
	}
}

void ShaderCompiler::compile() {
	find_dependencies();

	begin();

	Handle<ShaderNode> base_node = { INVALID_HANDLE };
		
	for (int i = 0; i < graph.nodes_manager.slots.length; i++) {
		ShaderNode* node = graph.nodes_manager.get(graph.nodes_manager.index_to_handle(i));
		
		if (node->type == ShaderNode::PBR_NODE) {
			base_node = graph.nodes_manager.index_to_handle(i);
			break;
		}
	}

	if (base_node.id != INVALID_HANDLE) {
		log("Shader Graph missing base node");
	}

	compile_node(base_node);

	end();
}


void compile_shader_graph(ShaderAsset* asset) {
	ShaderCompiler compiler(*asset->graph);
	compiler.compile();

	StringBuffer file_path = format("data/shader_output/", asset->name, ".frag");

	File file;
	if (file.open(file_path, File::WriteFile)) {
		log("Could not open file to write shader output");
	}

	file.write(compiler.contents);
	file.close();


	asset->handle = load_Shader("shaders/pbr.vert", file_path);

	insert_shader_handle_to_asset(asset);
	
	reload_shader(*RHI::shader_manager.get(asset->handle));
}

void new_node_popup(ShaderAsset* asset);

void ShaderEditor::render(World& world, Editor& editor, RenderParams& params, Input& input) {
	if (!this->open) return;

	AssetTab& tab = editor.asset_tab;

	if (ImGui::Begin("Shader Graph Editor", &this->open, ImGuiWindowFlags_NoScrollbar)) {
		if (this->current_shader == -1) {
			ImGui::Text("Please select a shader");
			ImGui::End();
			return;
		}

		ShaderAsset* asset = tab.assets.by_id<ShaderAsset>(current_shader);

		ImGui::SetNextItemWidth(ImGui::GetContentRegionMax().x * 0.8);
		render_name(asset->name, tab.default_font);

		ImGui::SameLine();
		if (ImGui::Button("Compile")) {
			compile_shader_graph(asset);
			render_preview(asset, world, editor, params);
		}

		if (input.key_pressed('R', true) && input.key_down(GLFW_KEY_LEFT_CONTROL, true)) {
			compile_shader_graph(asset);
			render_preview(asset, world, editor, params);
		}

		if (input.key_pressed('S', true) && input.key_down(GLFW_KEY_LEFT_SHIFT, true)) {
			set_scale(asset->graph.get(), input, 1.0f);
		}

		if (input.key_pressed('C', true) && input.key_down(GLFW_KEY_LEFT_SHIFT, true)) {
			asset->graph->scrolling = glm::vec2(0, 0);
			asset->graph->scale = 1.0f;
		}

		if (input.key_pressed('X')) {
			for (auto handle : asset->graph->selected) {
				asset->graph->nodes_manager.free(handle);
			}
		}

		float font_size = 32.0f * asset->graph->scale;

		
		if (font_size > 90.0f) {
			ImGui::PushFont(font[9]);
		}
		else {
			ImGui::PushFont(font[glm::max(0, (int)glm::ceil(font_size / 10.0f) - 1)]);

			//log("Scaling to font size ", font_size);
			//log("Got font size ", 10.0f * ((int)glm::ceil(font_size / 10.0f)));
		}
		
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f * asset->graph->scale, 2.0f * asset->graph->scale));

		asset->graph->render(world, editor, params, input);

		glm::vec2 mouse_drag_begin(ImGui::GetMousePos().x, ImGui::GetMousePos().y);
		mouse_drag_begin.x -= ImGui::GetMouseDragDelta().x;
		mouse_drag_begin.y -= ImGui::GetMouseDragDelta().y;

		glm::vec2 window_a(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y);
		glm::vec2 window_b(window_a.x + ImGui::GetWindowSize().x, window_a.y + ImGui::GetWindowSize().y);

		bool mouse_drag_within_window = window_a.x < mouse_drag_begin.x && window_a.y < mouse_drag_begin.y && window_b.x > mouse_drag_begin.x && window_b.y > mouse_drag_begin.y;

		if (ImGui::IsMouseDragging()) {
			log("here", window_a.x, window_b.x, window_a.y, window_b.y);
		}

		if (!ImGui::IsAnyItemHovered() && ImGui::IsMouseDragging() && asset->graph->action.type == ShaderGraph::Action::None && mouse_drag_within_window) {
			asset->graph->action.type = ShaderGraph::Action::BoxSelect;
			asset->graph->action.mouse_pos = glm::vec2(ImGui::GetMousePos().x, ImGui::GetMousePos().y);
		}

		if (asset->graph->action.type == ShaderGraph::Action::ApplyMove || asset->graph->action.type == ShaderGraph::Action::BoxSelectApply) {
			asset->graph->action.type = ShaderGraph::Action::None;
		}

		if (!ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(0) && asset->graph->action.type == ShaderGraph::Action::None && mouse_drag_within_window) {
			asset->graph->selected.clear();
		}

		if (asset->graph->action.type == ShaderGraph::Action::BoxSelect && !ImGui::IsMouseDragging()) {
			asset->graph->action.type = ShaderGraph::Action::BoxSelectApply;
			asset->graph->action.mouse_delta.x = ImGui::GetMouseDragDelta().x;
			asset->graph->action.mouse_delta.y = ImGui::GetMouseDragDelta().y;
		}

		if (ImGui::IsKeyPressed(GLFW_KEY_ESCAPE)) {
			asset->graph->action.type = ShaderGraph::Action::None;
		}

		if (ImGui::IsKeyPressed(GLFW_KEY_LEFT_SHIFT) && ImGui::IsKeyPressed(GLFW_KEY_D)) {
			//for () {
			//
			//}
		}

		ImGui::PopStyleVar();
		ImGui::PopFont();

		if (ImGui::GetIO().MouseClicked[1]) {
			ImGui::OpenPopup("NewNode");
		}

		new_node_popup(asset);
	}

	ImGui::End();
}

ShaderAsset* create_new_shader(World& world, AssetTab& self, Editor& editor, RenderParams& params) {
	ID id = self.assets.make_ID();
	ShaderAsset* asset = self.assets.make<ShaderAsset>(id);
	asset->name = "Shader Graph";

	add_asset(self, id);
	
	return NULL;
}

ShaderNode make_pbr_node();

void asset_properties(ShaderAsset* shader, Editor& editor, World& world, AssetTab& self, RenderParams& params) {
	render_name(shader->name, self.default_font);
	
	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Shader Graph");

	if (ImGui::Button("Open Shader Editor")) {
		editor.shader_editor.open = true;
		editor.shader_editor.current_shader = self.selected;
	}

	if (shader->graph == NULL) {
		shader->graph = std::unique_ptr<ShaderGraph>(new ShaderGraph());
		shader->graph->nodes_manager.make(make_pbr_node());
	}

	rot_preview(shader->graph->rot_preview);
	render_preview(shader, world, editor, params);
}