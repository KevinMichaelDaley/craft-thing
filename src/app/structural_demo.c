#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "dungeoncraft/gpu.h"
#include "ferrum/structural/gpu.h"
#include "structural_demo.h"

typedef struct {
    dc_gpu_t *gpu;
    fr_structural_gpu_graph_t graph;
    unsigned material;
    bool marked, braced;
    dc_gpu_overlay_t overlay;
} structural_demo_t;

static dc_gpu_body_shape_t rectangle(unsigned material, int x, int y, int w,
                                     int h) {
    return (dc_gpu_body_shape_t){.count = 4,
                                 .material = material,
                                 .vertices = {{x << 16, y << 16},
                                              {(x + w) << 16, y << 16},
                                              {(x + w) << 16, (y + h) << 16},
                                              {x << 16, (y + h) << 16}}};
}
static fr_structural_gpu_node_t scene_node(uint32_t id, int x, int y) {
    return (fr_structural_gpu_node_t){.id = id,
                                      .generation = 1,
                                      .chunk = {x / 64, y / 64},
                                      .x_fp = (x % 64) << 16,
                                      .y_fp = (y % 64) << 16};
}
static fr_structural_gpu_graph_t supported_frame(unsigned material,
                                                 bool marked) {
    fr_structural_gpu_graph_t graph = {.node_count = 5,
                                       .member_count = 3,
                                       .joint_count = 1,
                                       .anchor_count = 2};
    graph.nodes[0] = scene_node(1, 120, 80);
    graph.nodes[1] = scene_node(2, 136, 80);
    graph.nodes[2] = scene_node(3, 152, 80);
    graph.nodes[3] = scene_node(4, 160, 94);
    graph.nodes[4] = scene_node(5, 154, 80);
    graph.members[0] = (fr_structural_gpu_member_t){
        1, 1, 1, 2, FR_STRUCTURAL_WOOD, 5 << 16, 0, 0};
    graph.members[1] = (fr_structural_gpu_member_t){
        2, 1, 2, 3, FR_STRUCTURAL_WOOD, 5 << 16, 0, 0};
    graph.members[2] = (fr_structural_gpu_member_t){
        3, 1, 4, 5, FR_STRUCTURAL_WOOD, 4 << 16, 0, 0};
    graph.anchors[0] =
        (fr_structural_gpu_anchor_t){.id = 1,
                                     .generation = 1,
                                     .node = 1,
                                     .kind = FR_JOINT_MOMENT,
                                     .width = 1,
                                     .height = 1,
                                     .terrain_material = DC_MATERIAL_STONE,
                                     .normal_stiffness = 1e8,
                                     .shear_stiffness = 1e8,
                                     .rotational_stiffness = 1e8,
                                     .bearing_capacity = 1e7,
                                     .pullout_capacity = 1e7,
                                     .shear_capacity = 1e7,
                                     .moment_capacity = 1e7,
                                     .friction = .5};
    graph.anchors[0].chunk = (dc_chunk_coord_t){1, 1};
    graph.anchors[0].x = 55;
    graph.anchors[0].y = 16;
    graph.anchors[1] = graph.anchors[0];
    graph.anchors[1].id = 2;
    graph.anchors[1].node = 4;
    graph.anchors[1].chunk.x = 2;
    graph.anchors[1].x = 33;
    graph.anchors[1].y = 30;
    graph.joints[0] = (fr_structural_gpu_joint_t){.id = 1,
                                                  .generation = 1,
                                                  .a = 3,
                                                  .b = 5,
                                                  .kind = FR_JOINT_MOMENT,
                                                  .normal_stiffness = 1e8,
                                                  .shear_stiffness = 1e8,
                                                  .rotational_stiffness = 1e8,
                                                  .bearing_capacity = 1e7,
                                                  .pullout_capacity = 1e7,
                                                  .shear_capacity = 1e7,
                                                  .moment_capacity = 1e7,
                                                  .friction = .5};
    for (unsigned i = 0; i < 3; i++)
        graph.members[i].material = material;
    if (marked)
        for (unsigned i = 0; i < 2; i++) {
            graph.members[i].depth_fp = 6 << 16;
            graph.members[i].flags = FR_STRUCTURAL_MARK_TERRAIN;
        }
    return graph;
}
static bool scene_terrain(dc_gpu_t *gpu, bool marked, char *err) {
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk)
        return false;
    bool okay = true;
    for (unsigned slot = 0; okay && slot < DC_GPU_CHUNK_SLOTS && slot < 70;
         slot++) {
        memset(chunk, 0, sizeof(*chunk));
        chunk->coord = (dc_chunk_coord_t){slot % 10, slot / 10};
        for (unsigned y = 0; y < 64; y++)
            for (unsigned x = 0; x < 64; x++) {
                unsigned wx = slot % 10 * 64 + x, wy = slot / 10 * 64 + y;
                if (wy >= 100 || (wx == 119 && wy >= 80) ||
                    (wx == 161 && wy >= 94))
                    chunk->cells[y * 64 + x].material = DC_MATERIAL_STONE;
                if (marked && wx >= 120 && wx < 152 && wy >= 77 && wy < 83)
                    chunk->cells[y * 64 + x].material = DC_MATERIAL_STONE;
            }
        okay = dc_gpu_upload_chunk(gpu, slot, chunk, err, 256) &&
               dc_gpu_set_page(gpu, slot % 10, slot / 10, slot, err, 256);
    }
    free(chunk);
    return okay;
}

static bool reset_scene(structural_demo_t *demo, char *err) {
    dc_gpu_destroy(demo->gpu);
    demo->gpu = NULL;
    if (!dc_gpu_create_window(&demo->gpu, 640, 448, 640, 448,
                              "build/shaders/pattern.comp.spv", err, 256) ||
        !dc_gpu_set_viewport(demo->gpu, 80, 0, 160, 112) ||
        !dc_gpu_set_display_zoom(demo->gpu, 4) ||
        !dc_gpu_set_rigid_solver(demo->gpu, true) ||
        !dc_gpu_set_overlay(demo->gpu, demo->overlay) ||
        !scene_terrain(demo->gpu, demo->marked, err))
        return false;
    demo->graph = supported_frame(demo->material, demo->marked);
    demo->braced = true;
    if (!fr_gpu_place_structure(demo->gpu, &demo->graph, err, 256))
        return false;
    dc_gpu_compound_shape_t l = {.count = 2}, u = {.count = 3};
    l.pieces[0] = rectangle(DC_GPU_BODY_STONE, 0, 0, 3, 16);
    l.pieces[1] = rectangle(DC_GPU_BODY_STONE, 3, 13, 13, 3);
    u.pieces[0] = rectangle(DC_GPU_BODY_STONE, 0, 0, 3, 16);
    u.pieces[1] = rectangle(DC_GPU_BODY_STONE, 13, 0, 3, 16);
    u.pieces[2] = rectangle(DC_GPU_BODY_STONE, 3, 13, 10, 3);
    int y = 61 << 16;
    if (!demo->marked)
        y += 32768;
    dc_gpu_world_body_t left = {.chunk = {1, 0},
                                .body = {.x_fp = 56 << 16,
                                         .y_fp = y,
                                         .width = 16,
                                         .height = 16,
                                         .id = 1,
                                         .active = 1}};
    dc_gpu_world_body_t right = left;
    right.chunk.x = 2;
    right.body.x_fp = 8 << 16;
    right.body.id = 2;
    return dc_gpu_spawn_compound_body(demo->gpu, left, &l, err, 256) &&
           dc_gpu_spawn_compound_body(demo->gpu, right, &u, err, 256) &&
           dc_gpu_set_window_title(
               demo->gpu,
               "Structural bodies | B remove brace | R reset | 1 wood 2 stone "
               "3 mortar | M mark stone | O stress | Esc quit");
}
static bool demo_key(structural_demo_t *demo, SDL_Keycode key, bool *running,
                     char *err) {
    if (key == SDLK_ESCAPE || key == SDLK_q) {
        *running = false;
        return true;
    }
    if (key == SDLK_b && demo->braced) {
        demo->graph.node_count = 3;
        demo->graph.member_count = 2;
        demo->graph.anchor_count = 1;
        demo->graph.joint_count = 0;
        bool okay = fr_gpu_place_structure(demo->gpu, &demo->graph, err, 256);
        if (okay)
            demo->braced = false;
        return okay;
    }
    if (key == SDLK_o) {
        demo->overlay = (dc_gpu_overlay_t)((demo->overlay + 1u) % 5u);
        return dc_gpu_set_overlay(demo->gpu, demo->overlay);
    }
    if (key == SDLK_1 || key == SDLK_2 || key == SDLK_3 || key == SDLK_m ||
        key == SDLK_r) {
        if (key != SDLK_r) {
            demo->material = key == SDLK_1   ? FR_STRUCTURAL_WOOD
                             : key == SDLK_3 ? FR_STRUCTURAL_MORTAR
                                             : FR_STRUCTURAL_STONE;
            demo->marked = key == SDLK_m;
        }
        return reset_scene(demo, err);
    }
    return true;
}
static bool script_key(SDL_Keycode key) {
    SDL_Event event = {.type = SDL_KEYDOWN};
    event.key.keysym.sym = key;
    return SDL_PushEvent(&event) == 1;
}
static bool verify_collapse(structural_demo_t *demo, char *err) {
    uint32_t pixels[160 * 112];
    dc_gpu_world_body_t left, right;
    dc_gpu_body_motion_t a, b;
    if (!dc_gpu_readback(demo->gpu, pixels, 160 * 112, err, 256) ||
        !dc_gpu_read_world_body(demo->gpu, 1, &left, err, 256) ||
        !dc_gpu_read_world_body(demo->gpu, 2, &right, err, 256) ||
        !dc_gpu_read_body_motion(demo->gpu, 1, &a, err, 256) ||
        !dc_gpu_read_body_motion(demo->gpu, 2, &b, err, 256))
        return false;
    return 64. * left.chunk.y + left.body.y_fp / 65536. > 67.5 &&
           64. * right.chunk.y + right.body.y_fp / 65536. > 67.5 &&
           fabsf(a.mass - 234.9f) < .001f && fabsf(b.mass - 340.2f) < .001f;
}
int dc_structural_demo_run(bool scripted) {
    structural_demo_t demo = {.material = FR_STRUCTURAL_WOOD,
                              .overlay = DC_GPU_OVERLAY_STRUCTURAL_STRESS};
    char err[256] = {0};
    bool okay = reset_scene(&demo, err), running = okay;
    unsigned frame = 0;
    double accumulator = 0, interval = 1. / 60.;
    uint64_t previous = SDL_GetPerformanceCounter(),
             frequency = SDL_GetPerformanceFrequency();
    while (okay && running) {
        uint64_t start = SDL_GetPerformanceCounter();
        if (scripted) {
            if (frame == 60)
                okay = script_key(SDLK_b);
            if (frame == 241)
                okay = verify_collapse(&demo, err) && script_key(SDLK_m);
            if (frame == 243) {
                dc_gpu_body_motion_t beam;
                uint32_t pixels[160 * 112];
                okay = dc_gpu_readback(demo.gpu, pixels, 160 * 112, err, 256) &&
                       dc_gpu_read_body_motion(demo.gpu,
                                               FR_STRUCTURAL_BODY_ID_BASE + 1,
                                               &beam, err, 256) &&
                       fabsf(beam.mass - 259.2f) < .001f;
                for (unsigned press = 0; okay && press < 5; press++)
                    okay = script_key(SDLK_o);
            }
            if (frame == 244)
                okay = demo.overlay == DC_GPU_OVERLAY_STRUCTURAL_STRESS &&
                       script_key(SDLK_r);
            if (frame == 246)
                okay = script_key(SDLK_q);
        }
        SDL_Event event;
        while (okay && SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
            else if (event.type == SDL_KEYDOWN)
                okay = demo_key(&demo, event.key.keysym.sym, &running, err);
        }
        uint64_t now = SDL_GetPerformanceCounter();
        double elapsed = (double)(now - previous) / frequency;
        previous = now;
        if (elapsed > .25)
            elapsed = .25;
        accumulator += elapsed;
        unsigned steps = scripted ? 1u : (unsigned)(accumulator / interval);
        if (steps > 15)
            steps = 15;
        if (!scripted)
            accumulator -= steps * interval;
        if (okay && running)
            okay =
                dc_gpu_present_chunks_steps(demo.gpu, steps, err, sizeof(err));
        uint64_t spent = SDL_GetPerformanceCounter() - start,
                 budget = frequency / 60;
        if (spent < budget)
            SDL_Delay((uint32_t)((budget - spent) * 1000 / frequency));
        frame++;
    }
    dc_gpu_destroy(demo.gpu);
    if (!okay)
        fprintf(stderr, "Structural demo failed at frame%u: %s\n", frame, err);
    else if (scripted)
        printf("Native structural placement, marking, brace and stress "
               "controls passed\n");
    return okay ? 0 : 1;
}
