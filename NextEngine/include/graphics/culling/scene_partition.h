#pragma once

#include "engine/core.h"
#include "core/math/aabb.h"
#include <atomic>

#define MAX_NODES 500
#define MAX_MESH_INSTANCES 10000

struct Node {
	AABB aabb;
	uint offset;
	u16 count;
	u16 child_count;
	u16 child[8];
};

struct Partition {
	std::atomic<int> count = 0;
	std::atomic<int> node_count = 0;
	Node nodes[MAX_NODES];
};

struct ScenePartition : Partition {
	AABB aabbs[MAX_MESH_INSTANCES];
	int meshes[MAX_MESH_INSTANCES];
	glm::mat4 model_m[MAX_MESH_INSTANCES];
};