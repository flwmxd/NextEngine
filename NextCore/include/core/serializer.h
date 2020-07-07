#pragma once

#include "core/reflection.h"
#include <cstdint>
#include "core/container/string_buffer.h"
#include "core/container/sstring.h"


struct CORE_API SerializerBuffer {
	Allocator* allocator = &default_allocator;
	char* data = NULL;
	unsigned int index = 0; //also size
	unsigned int capacity = 0;

	void* pointer_to_n_bytes(unsigned int);
	void write_byte(uint8_t);
	void write_int(int32_t);
	void write_long(int64_t);
	void write_float(float);
	void write_double(double);
	void write_struct(refl::Struct*, void*);
	//void write_union(refl::TypeDescriptor_Union*, void*);
	void write_string(string_buffer&);
	void write_string(sstring&);
	void write_array(refl::Array*, void*);
	void write(refl::Type*, void*);

	~SerializerBuffer();
};

struct CORE_API DeserializerBuffer {
	DeserializerBuffer();
	DeserializerBuffer(const char* data, unsigned int length);

	const char* data;
	unsigned int index = 0;
	unsigned int length;

	const void* pointer_to_n_bytes(unsigned int);
	uint8_t read_byte();
	int32_t read_int();
	int64_t read_long();
	float read_float();
	double read_double();
	void read_struct(refl::Struct*, void*);
	//void read_union(refl::TypeDescriptor_Union*, void*);
	void read_string(string_buffer&);
	void read_string(sstring&);
	void read_array(refl::Array*, void*);
	void read(refl::Type*, void*);
};