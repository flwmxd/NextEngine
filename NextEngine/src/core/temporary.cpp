#include "stdafx.h"
#include "core/temporary.h"
#include <memory>

void TemporaryAllocator::clear() {
	this->occupied = 0;
}

TemporaryAllocator::TemporaryAllocator(size_t max_size) {
	this->occupied = 0;
	this->max_size = max_size;
	this->memory = ALLOC_ARRAY(char, max_size);
}

TemporaryAllocator::~TemporaryAllocator() {
	FREE_ARRAY(this->memory, max_size);
}

void* TemporaryAllocator::allocate(size_t size) {
	if (this->occupied > this->max_size) {
		throw "Temporary allocator out of memory";
	}
	
	void* ptr = this->memory + occupied;
	size_t space = this->max_size - occupied;
	ptr = std::align(16, size, ptr, space);
	space -= size;
	this->occupied = this->max_size - space;

	return ptr;
}

