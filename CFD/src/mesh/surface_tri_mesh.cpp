#include "mesh/surface_tet_mesh.h"

SurfaceTriMesh::SurfaceTriMesh() {}

SurfaceTriMesh::~SurfaceTriMesh() {
	free(edges);
	free(indices);
}

SurfaceTriMesh::SurfaceTriMesh(SurfaceTriMesh&& other) {
	tri_capacity = other.tri_capacity;
	tri_count = other.tri_count;
	positions = std::move(other.positions);
	edges = other.edges;
	indices = other.indices;
	aabb = other.aabb;

	other.edges = nullptr;
	other.indices = nullptr;
}

SurfaceTriMesh::SurfaceTriMesh(const SurfaceTriMesh& other) {
	tri_capacity = other.tri_capacity;
	tri_count = other.tri_count;
	positions = other.positions;

	uint n = tri_count * 3;
	edges = new edge_handle[n];
	indices = new vertex_handle[n];

	memcpy_t(edges, other.edges, n);
	memcpy_t(indices, other.indices, n);

	aabb = other.aabb;
}

void SurfaceTriMesh::operator=(SurfaceTriMesh&& other) {
	//todo free previous
	
	tri_capacity = other.tri_capacity;
	tri_count = other.tri_count;
	positions = std::move(other.positions);
	edges = other.edges;
	indices = other.indices;
	aabb = other.aabb;

	other.edges = nullptr;
	other.indices = nullptr;
}

void SurfaceTriMesh::reserve_tris(uint count) {
	if (count < tri_capacity) return;

	tri_capacity = count;
	edges = realloc_t(edges, count*3);
	indices = realloc_t(indices, count*3);
}

void SurfaceTriMesh::resize_tris(uint count) {
	reserve_tris(count);
	for (uint i = 0; i < count*3; i++) {
		indices[i] = {};
		edges[i] = {};
	}
	tri_count = count;
}