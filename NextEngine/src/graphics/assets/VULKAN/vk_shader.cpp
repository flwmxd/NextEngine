#include "stdafx.h"

#ifdef RENDER_API_VULKAN

#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/rhi/vulkan/shader.h"
#include "graphics/assets/shader.h"

#include "core/io/vfs.h"
#include "core/io/logger.h"
#include <stdio.h>
#include "core/container/tvector.h"
#include <glm/gtc/type_ptr.hpp>
#include <shaderc/shaderc.h>
#include <spirv-reflect/spirv_reflect.h>

REFLECT_STRUCT_BEGIN(ShaderInfo)
REFLECT_STRUCT_MEMBER(vfilename)
REFLECT_STRUCT_MEMBER(ffilename)
REFLECT_STRUCT_END()

VkShaderModule make_ShaderModule(VkDevice device, string_view code) {
	VkShaderModuleCreateInfo makeInfo = {};
	makeInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	makeInfo.codeSize = code.length;
	makeInfo.pCode = (const unsigned int*)code.data;

	VkShaderModule shaderModule;

	VK_CHECK(vkCreateShaderModule(device, &makeInfo, nullptr, &shaderModule))

	return shaderModule;
}

string_buffer preprocess_source(Assets&, string_view source, shader_flags flags);

tvector<string_view> preprocess_lex(string_view source) {
	tvector<string_view> tokens;
	string_view tok;
	int i = 0;
	
	tok.data = source.data;

	for (i = 0; i < source.length; i++) {
		char c = source[i];

		if (c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == ';' || c == '}' || c == '(' || c == ')' || c == '=' || c == ',') {
			if (tok.length > 0) tokens.append(tok);

			tok.data = &source.data[i + 1];
			tok.length = 0;

			tokens.append(string_view(&source.data[i], 1));
		}
		else tok.length++;
	}

	if (tok.length > 0) {
		tokens.append(tok);
	}

	return tokens;
}

enum MaterialInputType {
	Channel1,
	Channel2,
	Channel3,
	Float,
	Int,
	Vec2,
	Vec3,
	Vec4
};

string_buffer preprocess_gen(Assets& assets, slice<string_view> tokens, shader_flags flags) {
	vector<string_view> already_included;
	vector<string_view> channels;

	string_buffer output;
	string_buffer in_struct_prefix;
	output.allocator = &temporary_allocator;
	output = "#version 450\n#extension GL_ARB_separate_shader_objects : enable\n";

	if (flags & SHADER_INSTANCED) output += "#define IS_INSTANCED\n";
	if (flags & SHADER_DEPTH_ONLY) output += "#define IS_DEPTH_ONLY\n";

	output += "#line 0\n";

	int line_count = 0;

	for (int i = 0; i < tokens.length; i++) {
		if (tokens[i] == "#include") {
			while (tokens[++i] == " ");

			auto name = tokens[i];

			if (!already_included.contains(name)) {
				string_buffer src;
				src.allocator = &temporary_allocator;

				if (!readf(assets, name, &src)) {
					fprintf(stderr, "Could not #include source file %s\n", name);
					return "";
				}

				//todo recursively apply the transformation
				output += src;
				output += tformat("\n#line ", line_count, "\n");
			}
		}
		else if (tokens[i] == "texture") {
			i++;
			string_view sampler = tokens[++i];

			if (channels.contains(sampler)) {
				output += sampler;
				output += "_scalar";
				output += " * ";
			}
			output += "texture(";
			output += sampler;
		}
		else if (tokens[i] == "struct") {
			i++;

			string_view name = tokens[++i];

			in_struct_prefix = tformat(name, ".");

			if (name == "Material") {
				while (tokens[++i] == " ");

				assert(tokens[i] == "{");

				tvector<MaterialInputType> types;
				tvector<string_view> names;

				i++;

				while (tokens[i] != "}") {
					while (tokens[i] == " " || tokens[i] == "\n" || tokens[i] == "\r" || tokens[i] == "\t") {
						if (tokens[i] == "\n") line_count++;
						i++;
					}

					string_view channel_type = tokens[i];

					while (tokens[++i] == " ");

					string_view name = tokens[i];
					string_buffer full_name = tformat(in_struct_prefix, name);

					int type = -1;
					if (channel_type == "channel1") type = Channel1;
					if (channel_type == "channel2") type = Channel2;
					if (channel_type == "channel3") type = Channel3;
					if (channel_type == "float") type = Float;
					if (channel_type == "int") type = Int;
					if (channel_type == "vec2") type = Vec2;
					if (channel_type == "vec3") type = Vec3;
					if (channel_type == "vec4") type = Vec4;

					if (type == -1) throw "Expecting channel type!";

					names.append(name);
					types.append((MaterialInputType)type);
					channels.append(name);

					if (tokens[++i] != ";") throw "expecting semi colon";

					while (tokens[++i] == " " || tokens[i] == "\n" || tokens[i] == "\r" || tokens[i] == "\t") {
						if (tokens[i] == "\n") line_count++;
					}
				}

				if (tokens[++i] != ";") throw "expecting semi colon";

				output += "layout (std140, set = 1, binding = 0) uniform Material {\n";

				for (int i = 0; i < names.length; i++) {
					int type_index = (int)types[i];
					string_view scalar_types[] = { "float", "vec2", "vec3", "float", "int", "vec2", "vec3", "vec4" };

					if (type_index > Channel3) {
						output += tformat("\t", scalar_types[type_index], " ", names[i], ";\n");
					}
					else {
						output += tformat("\t", scalar_types[type_index], " ", names[i], "_scalar", ";\n");
					}
				}

				output += "};\n";

				for (int i = 0; i < names.length; i++) {
					int type_index = (int)types[i];
					if (type_index > Channel3) continue;
					output += tformat("layout (set = 1, binding = ", i + 1, ") uniform sampler2D ", names[i], ";\n");
				}
			}
			else {
				output += "struct ";
				output += name;
			}
		}
		else if (tokens[i] == "}") {
			in_struct_prefix = "";
			output += "}";
		}
		else {
			if (tokens[i] == "\n") line_count++;
			output += tokens[i];
		}
	}

	return output;
}


string_buffer preprocess_source(Assets& assets, string_view source, shader_flags flags) { //todo refactor into struct
	tvector<string_view> tokens = preprocess_lex(source);
	return preprocess_gen(assets, tokens, flags);
}


string_buffer compile_glsl_to_spirv(Assets& assets, shaderc_compiler_t compiler, Stage stage, string_view source, string_view input_file_name, shader_flags flags, string_buffer* err) {
	string_buffer source_assembly = preprocess_source(assets, source, flags);

	printf("Source: %s", source_assembly.data);

	shaderc_shader_kind glsl_shader_kind = stage == VERTEX_STAGE ? shaderc_glsl_vertex_shader : shaderc_glsl_fragment_shader;

	shaderc_compilation_result_t result = shaderc_compile_into_spv(compiler, source_assembly.data, source_assembly.length, glsl_shader_kind, input_file_name.c_str(), "main", NULL);

	if (shaderc_result_get_num_errors(result) > 0) {
		*err = shaderc_result_get_error_message(result);
		shaderc_result_release(result);
		return "";
	}


	uint length = shaderc_result_get_length(result);
	const char* bytes = shaderc_result_get_bytes(result);

	string_buffer buffer;
	buffer.allocator = &temporary_allocator;
	buffer.reserve(length);
	buffer.length = length;

	memcpy(buffer.data, bytes, length);

	shaderc_result_release(result); //todo this is dumb, why copy the buffer again, just to have a simpler output

	return buffer;
}

struct ShaderReflection {
	array<10, SpvReflectDescriptorSet*> descriptor_sets;
	array<10, SpvReflectBlockVariable*> push_constant_blocks;
};



void reflect_extract(SpvReflectShaderModule& compiler, ShaderReflection& ref, string_view spirv) {
	SpvReflectResult result = spvReflectCreateShaderModule(spirv.length, spirv.data, &compiler);
	if (!result == SPV_REFLECT_RESULT_SUCCESS) {
		printf("Reflection parser, could not parse SPIRV!");
		abort();
	}

	spvReflectEnumerateDescriptorSets(&compiler, &ref.descriptor_sets.length, NULL);
	spvReflectEnumeratePushConstantBlocks(&compiler, &ref.push_constant_blocks.length, NULL);

	assert(ref.descriptor_sets.length <= 10);
	assert(ref.push_constant_blocks.length <= 10);

	spvReflectEnumerateDescriptorSets(&compiler, &ref.descriptor_sets.length, ref.descriptor_sets.data);
	spvReflectEnumeratePushConstantBlocks(&compiler, &ref.push_constant_blocks.length, ref.push_constant_blocks.data);
}

void write_joint_ref(ShaderModuleInfo* info, VkShaderStageFlagBits stage, ShaderReflection& ref) {
	for (auto descriptor : ref.descriptor_sets) {
		uint set = descriptor->set;

		if (set >= info->sets.length) {
			info->sets.resize(set + 1);
			//Leaves a gap in the array for descriptor sets the shader doesn't write too
		}

		DescriptorSetInfo& set_info = info->sets[set];

		if (descriptor->binding_count >= set_info.bindings.length) {
			set_info.bindings.resize(descriptor->binding_count);
		}

		for (uint i = 0; i < descriptor->binding_count; i++) {
			auto binding = descriptor->bindings[i];
			DescriptorBindingInfo& out = set_info.bindings[i];

			assert(binding->binding < MAX_BINDING);

			if (out.stage == 0) {
				out.binding = binding->binding;
				out.count = 1; 
				out.name = binding->name;
				out.type = (VkDescriptorType)binding->descriptor_type;

				for (uint32_t i_dim = 0; i_dim < binding->array.dims_count; ++i_dim) {
					out.count *= binding->array.dims[i_dim];
				}
				
				if (out.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
					UBOInfo ubo_info = {};
					ubo_info.id = binding->binding;

					auto& block = binding->block;

					ubo_info.type_name = binding->type_description->type_name;
					ubo_info.size = block.size;

					for (uint i = 0; i < block.member_count; i++) {
						UBOFieldInfo field = {};
						field.name = block.members[i].name;
						field.offset = block.members[i].offset;
						field.size = block.members[i].size;

						ubo_info.fields.append(field);
					}

					set_info.ubos.append(ubo_info);
				}

				if (out.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
					SamplerInfo sampler_info = {};
					sampler_info.id = binding->binding;
		
					set_info.samplers.append(sampler_info);
				}
				
			}
			else {
				//todo check if two bindings are compatible
			}

			out.stage = (VkShaderStageFlagBits)(out.stage | stage);
		}

	}
}

void print_module(ShaderModuleInfo& info) {
	int set_index = 0;
	for (int set_i = 0; set_i < info.sets.length; set_i++) {
		DescriptorSetInfo& descriptor = info.sets[set_i];

		printf("SET %i\n", set_i);

		uint uniform_index = 0;

		for (int binding_i = 0; binding_i < descriptor.bindings.length; binding_i++) {
			DescriptorBindingInfo& binding = descriptor.bindings[binding_i];

			if (binding.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
				printf("\tBINDING %i sampler2D %s %\n", binding.binding, binding.name);
			}
				
			if (binding.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
				UBOInfo& ubo = descriptor.ubos[uniform_index++];
				
				printf("\tBINDING %i uniform %s{\n", binding.binding, ubo.type_name);

				for (UBOFieldInfo& field : ubo.fields) {
					printf("\t\tFIELD %s;\n", field.name);
				}

				printf("\t}\n");
			}
		}

		printf("\n");
	}
}

//EXPECTS INFO TO BE ZERO INITIALIZED
void reflect_module(ShaderModuleInfo& info, string_view vert_spirv, string_view frag_spirv) {
	SpvReflectShaderModule vert_module;
	SpvReflectShaderModule frag_module;

	ShaderReflection vert;
	ShaderReflection frag;

	reflect_extract(vert_module, vert, vert_spirv);
	reflect_extract(frag_module, frag, frag_spirv);

	write_joint_ref(&info, VK_SHADER_STAGE_VERTEX_BIT, vert);
	write_joint_ref(&info, VK_SHADER_STAGE_FRAGMENT_BIT, frag);

	printf("===========\n");
	
	print_module(info);

	spvReflectDestroyShaderModule(&vert_module);
	spvReflectDestroyShaderModule(&frag_module);
}

void gen_descriptors(ShaderModuleInfo& info) {
	int set_index = 0;
	for (int set_i = 0; set_i < info.sets.length; set_i++) {
		DescriptorSetInfo& descriptor = info.sets[set_i];
			 
		VkDescriptorSetLayoutCreateInfo layout_info = {};
		layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layout_info.bindingCount = descriptor.bindings.length;

		array<20, VkDescriptorSetLayoutBinding> layout_bindings(descriptor.bindings.length); 

		for (int binding_i = 0; binding_i < descriptor.bindings.length; binding_i++) {
			DescriptorBindingInfo& binding = descriptor.bindings[binding_i];

			VkDescriptorSetLayoutBinding& layout_binding = layout_bindings[binding_i];
			layout_binding.binding = binding.binding;
			layout_binding.descriptorCount = binding.count;
			layout_binding.descriptorType = binding.type; 

			layout_binding.stageFlags = binding.stage;
		}
		
	}
}


//void ShaderConfig::bind() {
//	/**/
//}


/*uniform_handle location(Shader* shader, string_view name) {
	for (unsigned int i = 0; i < shader->uniforms.length; i++) {
		if (shader->uniforms[i].name == name) {
			return { i };
		}
	}

	uniforms.append(Uniform(name)); //todo uniforms should have reflection data

	for (auto& config : configurations) {
		config.uniform_bindings.append(0); //glGetUniformLocation(config.id, name.c_str()));
	}

	return { uniforms.length - 1 };
}

uniform_handle location(ShaderManager& manager, shader_handle handle, string_view name) {
	return location(manager.get(handle), name);
}

shader_config_handle get_config_handle(ShaderManager& manager, shader_handle shader_handle, ShaderConfigDesc& desc) {
	Shader* shader = get(shader_handle);
	
	for (unsigned int i = 0; i < shader->configurations.length; i++) {
		if (shader->configurations[i].desc == desc) return { i };
	}

	ShaderConfig config;
	config.desc = desc;
	
	string_buffer err;
	if (!load_in_place_with_err(level, &config, &err, shader->v_filename, shader->f_filename, shader, false)) {
		throw err;
	}

	config.uniform_bindings.reserve(shader->uniforms.length);
	for (auto& uniform : shader->uniforms) {
		config.uniform_bindings.append(0); //glGetUniformLocation(config.id, uniform.name.c_str()));
	}

	shader->configurations.append(std::move(config));

	return { shader->configurations.length - 1 };
}


Uniform::Uniform(string_view name) : name(name)
{
}

Uniform* get_uniform(ShaderManager& manager, shader_handle shader_handle, uniform_handle uniform_handle) {
	Shader* shader = manager.get(shader_handle);
	return &shader->uniforms[uniform_handle.id];
}

int get_uniform_binding(Assets& assets, uniform_handle uniform_handle) {
	return 0; //shader->uniform_bindings[uniform_handle.id];
}

int get_uniform_binding(Assets& assets, const char* uniform) {
	return 0;
}


void gl_bind(ShaderManager& manager, shader_handle shader, shader_config_handle config) {
	manager.get_config(shader, config)->bind();
}

void make_ShaderCompiler() {
	auto shader_compiler = shaderc_compiler_initialize();
}

void destroy_ShaderCompiler() {
	shaderc_compiler_release(shader_compiler);
}



void reload(Assets& assets, shader_handle handle) {
	Shader& shad_factory = *get(handle);

	log("recompiled shader: ", shad_factory.v_filename);

	for (auto& shad : shad_factory.configurations) {
		auto previous = shad.id;

		string_buffer err;
		if (load_in_place_with_err(level, &shad, &err, shad_factory.v_filename, shad_factory.f_filename, &shad_factory, false)) {

			for (int i = 0; i < shad.uniform_bindings.length; i++) {
				shad.uniform_bindings[i] = 0; 
			}
		}
		else {
			log("Error ", err);
		}
	}
}

void ShaderManager::reload_modified() {
	for (int i = 0; i < slots.length; i++) {
		auto& shad_factory = slots[i];

		if (shad_factory.v_filename.length() == 0) continue;

		auto v_time_modified = level.time_modified(shad_factory.v_filename);
		auto f_time_modified = level.time_modified(shad_factory.f_filename);

		if (v_time_modified > shad_factory.v_time_modified or f_time_modified > shad_factory.f_time_modified) {
			reload(index_to_handle(i));
			
			shad_factory.v_time_modified = v_time_modified;
			shad_factory.f_time_modified = f_time_modified;
		}
	}
}
*/
#endif