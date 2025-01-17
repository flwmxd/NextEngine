#pragma once

#include "core/math/vec4.h"

using tet_handle = uint;

const tet_handle NEIGH = ((1 << 30)-1) << 2;
const tet_handle NEIGH_FACE = (1<<2)-1;

struct TetMesh {
    uint tet_count;
    vertex_handle* indices;
    tet_handle* faces;
};

struct Delaunay;
struct CFDVolume;
struct CFDSurface;
struct CFDDebugRenderer;
struct SurfaceTriMesh;

Delaunay* make_Delaunay(CFDVolume&, const AABB&, CFDDebugRenderer&);
void destroy_Delaunay(Delaunay*);
void generate_n_layers(Delaunay& d, CFDSurface& surface, uint n, float initial, float g, float resolution, uint layers, float min_quad_quality);
TetMesh generate_uniform_tri_mesh(Delaunay& d, CFDSurface& surface, float size);

//should this be exposed?
bool add_vertices(Delaunay& d, slice<vertex_handle> verts, float min_dist = 0.0f);
bool refine(Delaunay& d, float size = FLT_MAX);
bool smooth(Delaunay& d);

struct InputModelBVH;

void constrain_triangulation(Delaunay& d, slice<Boundary> boundaries, InputModelBVH& bvh);
void remove_super(Delaunay& d);
bool complete(Delaunay& d);
