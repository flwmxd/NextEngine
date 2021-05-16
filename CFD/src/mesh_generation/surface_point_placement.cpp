#include "mesh/surface_tet_mesh.h"
#include "mesh_generation/surface_point_placement.h"
#include "mesh_generation/cross_field.h"
#include "mesh_generation/point_octotree.h"
#include "mesh/input_mesh_bvh.h"
#include "numerical/spline.h"
#include "mesh/feature_edges.h"

#include "visualization/debug_renderer.h"

#include "geo/predicates.h"

#include <glm/gtx/rotate_vector.hpp>
#include "visualization/color_map.h"
#include "core/profiler.h"

#include <deque>

SurfacePointPlacement::SurfacePointPlacement(SurfaceTriMesh& tri_mesh, SurfaceCrossField& cross, PointOctotree& octotree, slice<float> curvatures)
: mesh(tri_mesh), cross_field(cross), octo(octotree), curvatures(curvatures) {

}

void draw_point(CFDDebugRenderer& debug, vec3 pos, vec4 color) {
    draw_line(debug, pos - vec3(0,0.01,0), pos + vec3(0,0.01,0), color);
    //draw_line(debug, pos - vec3(0.1,0,0), pos + vec3(0.1,0,0), color);
    //draw_line(debug, pos - vec3(0,0,0.1), pos + vec3(0,0,0.1), color);
}

vertex_handle add_vertex(PointOctotree& octo, vec3 pos) {
    vertex_handle v = {(int)octo.positions.length};
    octo.positions.append(pos);
    octo.add_vert(v);
    return v;
}

float distance_to_triangle(vec3 v[3], vec3 pos) {
    vec3 center = (v[0] + v[1] + v[2]) / 3.0f;
    vec3 normal = triangle_normal(v);
    
    vec3 project = pos - dot(pos - center, normal)*normal;
    
    vec3 c0 = (v[0]+v[1])/2;
    vec3 c1 = (v[1]+v[2])/2;
    vec3 c2 = (v[2]+v[0])/2;
    
    vec3 e0 = v[1]-v[0];
    vec3 e1 = v[2]-v[1];
    vec3 e2 = v[0]-v[2];
    
    vec3 b0 = -normalize(cross(normal, e0));
    vec3 b1 = -normalize(cross(normal, e1));
    vec3 b2 = -normalize(cross(normal, e2));
    
    float disp_e0 = dot(project - c0, b0);
    float disp_e1 = dot(project - c1, b1);
    float disp_e2 = dot(project - c2, b2);
    
    float distance = FLT_MAX;
    if (disp_e0 > 0 && disp_e0 < distance) distance = disp_e0;
    else if (disp_e1 > 0 && disp_e1 < distance) distance = disp_e1;
    else if (disp_e2 > 0 && disp_e2 < distance) distance = disp_e2;
    else distance = 0.0f; //inside
    
    return distance;
}



tri_handle search_tri(SurfaceTriMesh& mesh, SurfaceCrossField& cross_field, CFDDebugRenderer& debug, tri_handle start, vec3* pos_ptr) {
    vec3 pos = *pos_ptr;
    
    tri_handle current = start;
    tri_handle last = 0;
    uint counter = 5;
    
    const uint n = 20;
#ifdef DEBUG_TRACE_TRISEARCH
    array<n, vec3> path;
    path.append(mesh.triangle_center(current));
#endif

    while (true) {
        vec3 v[3];
        mesh.triangle_verts(current, v);
        vec3 normal = triangle_normal(v);
        vec3 center = (v[0]+v[1]+v[2])/3.0f;
        
        float disp = dot(pos - center, normal);
        vec3 project = pos - disp*normal;

        vec3 e0 = v[1]-v[0];
        vec3 e1 = v[2]-v[1];
        vec3 e2 = v[0]-v[2];
    
        vec3 c0 = project - v[0];
        vec3 c1 = project - v[1];
        vec3 c2 = project - v[2];
    
        bool inside = dot(normal, cross(e0, c0)) > 0 &&
                      dot(normal, cross(e1, c1)) > 0 &&
                      dot(normal, cross(e2, c2)) > 0;
        
        if (inside) {
            *pos_ptr = project;
            return current;
        }
        
        float min_length = FLT_MAX; // sq(project - center);
        tri_handle closest_neighbor = 0;
        
        for (uint j = 0; j < 3; j++) {
            tri_handle tri = mesh.neighbor(current, j);
            if (tri == last) continue;
            
            vec3 v0 = v[j];
            vec3 v1 = v[(j+1)%3];
            vec3 e = v0-v1;
            vec3 c = (v0+v1)/2.0f;
            
            //Plane test with edge
            vec3 b = normalize(cross(normal, e));
            float distance_to_edge = dot(project - c, b);
            
            //sq(pos - center);
            if (distance_to_edge > 0 && distance_to_edge < min_length) {
                closest_neighbor = tri;
                min_length = distance_to_edge;
            }
        }
     
        if (closest_neighbor) {
            last = current;
            current = closest_neighbor;
            
#ifdef DEBUG_TRACE_TRISEARCH
            path.append(center);
#endif
            
            if (++counter >= n) {
#ifdef DEBUG_TRACE_TRISEARCH
                for (uint i = 0; i < path.length-1; i++) {
                    draw_line(debug, path[i], path[i+1], vec4(0,0,1,1));
                }
                draw_line(debug, path[path.length-1], pos, vec4(0,1,0,1));

                draw_point(debug, path[0], vec4(0,1,0,1));
                draw_point(debug, pos, vec4(0,0,1,1));
#endif                
                //suspend_execution(debug);
                
                return 0;
            }
        } else {
            break;
        }
    }
       
    return 0;
}

struct QMorphFront {

};

void propagate_points(PointOctotree& octo, SurfaceTriMesh& mesh, SurfaceCrossField& cross_field, CFDDebugRenderer& debug, slice<FeatureCurve> curves);

void link_edge(SurfaceTriMesh& mesh, edge_handle edge, edge_handle neigh) {
    mesh.edges[edge] = neigh;
    mesh.edges[neigh] = edge;

    vertex_handle v0, v1;
    mesh.edge_verts(neigh, &v0, &v1);

    mesh.indices[edge] = v1;
    mesh.indices[TRI_NEXT_EDGE(edge)] = v0;
}

void draw_triangle(CFDDebugRenderer& debug, SurfaceTriMesh& mesh, tri_handle tri, vec4 color) {
    vec3 points[3];
    mesh.triangle_verts(tri, points);
    vec3 normal = triangle_normal(points);
    
    //draw_line(debug, points[0], points[1], color);
    //draw_line(debug, points[1], points[2], color);
    //draw_line(debug, points[2], points[0], color);

    for (uint i = 0; i < 3; i++) points[i] += normal * 0.001;
    draw_triangle(debug, points, color);
}

void SurfacePointPlacement::propagate(slice<FeatureCurve> curves, CFDDebugRenderer& debug) {
	//vector<FeatureEdge> features = cross_field.get_feature_edges();
    
    QMorphFront front;
    
    float mesh_size = 0.1;

    SurfaceTriMesh result = mesh;
    
    for (FeatureCurve curve : curves) {
        Spline spline = curve.spline;

        float length = spline.total_length;
        uint n = length / mesh_size;

        //printf("============== %i\n", spline.points());

        uint last_segment = UINT_MAX;
        edge_handle last_edge = 0;

        for (uint i = 0; i <= n; i++) {
            float t = spline.const_speed_reparam(i, mesh_size);
            vec3 pos = spline.at_time(t);
            vec3 tangent = spline.tangent(t);

            uint segment = (uint)t;

            edge_handle e[2];
            e[0] = curve.edges[segment];
            e[1] = mesh.edges[e[0]];

            vec3 bitangent0 = cross_field.at_edge(e[0]).bitangent;
            vec3 bitangent1 = cross_field.at_edge(e[1]).bitangent;

            if (last_segment == segment) {
                e[0] = last_edge;
                e[1] = result.edges[last_edge];
            }

            vec3 v0 = result.position(e[0]);
            vec3 v1 = result.position(e[1]);

            float edge_length = ::length(v0 - v1);

            tri_handle t0 = TRI(e[0]);
            tri_handle t1 = TRI(e[1]);

            float dist1 = sq(v0 - pos);
            float dist2 = sq(v1 - pos);

            float threshold = edge_length * 0.4;

            last_edge = e[0];
            last_segment = segment;

            draw_triangle(debug, result, t0, vec4(1));
            draw_triangle(debug, result, t1, vec4(1));

            if (dist1 < threshold) result.positions[result.indices[e[0]].id] = pos;
            else if (dist2 < threshold) result.positions[result.indices[e[1]].id] = pos;
            else {
                //split triangle
                tri_handle t2 = result.alloc_tri();
                tri_handle t3 = result.alloc_tri();

                vertex_handle v = { result.positions.length };
                result.positions.append(pos);

                edge_handle en0 = TRI_NEXT_EDGE(e[0]);
                edge_handle en1 = TRI_NEXT_EDGE(e[1], 2);

                result.indices[t2] = v;
                result.indices[t3] = v;

                link_edge(result, t2, t3 + 2);
                link_edge(result, t2 + 1, result.edges[en0]);
                link_edge(result, t3 + 1, result.edges[en1]);
                link_edge(result, t2 + 2, en0);
                link_edge(result, t3, en1);

                draw_triangle(debug, result, t2, vec4(1));
                draw_triangle(debug, result, t3, vec4(1));

                last_edge = t2;
            }

            suspend_execution(debug);
        }
    }

    suspend_execution(debug);
    //propagate_points(octo, mesh, cross_field, debug, curves);
}

void propagate_points(PointOctotree& octo, SurfaceTriMesh& mesh, SurfaceCrossField& cross_field, CFDDebugRenderer& debug, slice<FeatureCurve> curves) {
    struct Point {
        tri_handle tri;
        vec3 position;
        float dist;
        float last_dt;
    };

    uint watermark = octo.positions.length;

    std::deque<Point> point_queue;

    //suspend_execution(debug);
    //clear_debug_stack(debug);

    float mesh_size = 1.0;
    float dist_mult = 0.0;

    /*

    for (uint i = 1; i < mesh.positions.length; i++) {
        vec3 pos = mesh.positions[i];
        float curv = curvatures[i];

        draw_point(debug, pos, color_map(fabs(curv)/1.0));
    }

    suspend_execution(debug);*/

    Profile discretize("Discretize curves");

    for (FeatureCurve curve : curves) {
        Spline spline = curve.spline;

        float length = spline.total_length;
        uint n = length / mesh_size;

        //printf("============== %i\n", spline.points());

        for (uint i = 0; i < n; i++) {
            float t = spline.const_speed_reparam(i, mesh_size);
            vec3 pos = spline.at_time(t);
            vec3 tangent = spline.tangent(t);

            edge_handle e0 = curve.edges[(uint)t];
            edge_handle e1 = mesh.edges[e0];

            vec3 normal0 = cross_field.at_edge(e0).normal;
            vec3 normal1 = cross_field.at_edge(e1).normal;

            vec3 bitangent0 = normalize(cross(normal0, tangent));
            vec3 bitangent1 = normalize(cross(normal1, tangent));

            add_vertex(octo, pos);
            //draw_point(debug, pos, vec4(1,0,0,1));
            //draw_line(debug, pos, pos + tangent * mesh_size, vec4(1,0,0,1));
            //draw_line(debug, pos, pos - tangent * mesh_size, vec4(1,0,0,1));

            //draw_line(debug, pos, pos - bitangent0 * mesh_size, vec4(0,0,1,1));
            //draw_line(debug, pos, pos + bitangent1 * mesh_size, vec4(0,0,1,1));

            Point point0{ TRI(e0), pos - bitangent0 * mesh_size, mesh_size, mesh_size };
            Point point1{ TRI(e1), pos + bitangent1 * mesh_size, mesh_size, mesh_size };

            point_queue.push_back(point0);
            point_queue.push_back(point1);
        }
    }

    discretize.log();

    //suspend_execution(debug);

    Profile place_points("Place points");

    while (!point_queue.empty()) {
        Point point = point_queue.front();
        point_queue.pop_front();

        tri_handle tri = search_tri(mesh, cross_field, debug, point.tri, &point.position);
        if (!tri) continue;

        //float curvature = curvatures[mesh.indices[tri].id];
        float last_dt = point.last_dt;
        float dt = mesh_size * (1.0 + dist_mult * point.dist); // / (1 + fabs(curvature));

        Cross cross = cross_field.at_tri(tri, point.position);

        vertex_handle found = octo.find_closest(point.position, cross, 0.7 * (point.last_dt + dt) / 2.0f);
        if (found.id != -1) {
            //draw_point(debug, point.position, vec4(0,0,1,1));
            continue;
        }

        //suspend_execution(debug);

        add_vertex(octo, point.position);
        //draw_point(debug, point.position, vec4(1,0,0,1)); // color_map(dt));

        //cross.bitangent = normalize(::cross(cross.normal, cross.tangent));

        Point points[4] = {};
        points[0].position = point.position + cross.tangent * dt;
        points[1].position = point.position - cross.tangent * dt;
        points[2].position = point.position + cross.bitangent * dt;
        points[3].position = point.position - cross.bitangent * dt;

        for (uint i = 0; i < 4; i++) {
            points[i].tri = tri;
            points[i].dist = point.dist + dt;
            points[i].last_dt = dt;
            //draw_point(debug, points[i].position, vec4(0,0,1,1));
            point_queue.push_back(points[i]);
        }

        //point_queue += {points,4};
    }

    place_points.log();

    for (uint i = watermark; i < octo.positions.length; i++) {
        draw_point(debug, octo.positions[i], vec4(1, 0, 0, 0));
    }
}