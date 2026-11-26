#include <math.h>
#include <stdio.h>

#include <SDL.h>

#include "../vulkan/gpu_internal.h"
#include "level_internal.h"
#include "player_view.h"

enum { CAMERA_DEAD_ZONE = 8, CAMERA_MAX_PAN = 1024, STICK_AIM_REACH = 80 };

static double view_axis(int64_t chunk, int64_t origin, int32_t position, uint32_t offset) {
    /* Subtract in integer space so distant chunks retain local cell precision. */
    double delta = chunk >= origin
        ? (double)((uint64_t)chunk - (uint64_t)origin)
        : -(double)((uint64_t)origin - (uint64_t)chunk);
    return delta * DC_CHUNK_SIDE + position / (double)DC_FLUID_FULL - offset;
}

static bool body_position(dc_level_view_t *view, uint32_t id, bool center,
                           double *x, double *y) {
    dc_gpu_world_body_t body;
    if (!id || !dc_gpu_read_world_body(view->gpu, id, &body, NULL, 0)) return false;
    *x = view_axis(body.chunk.x, view->origin.x, body.body.x_fp, view->camera_offset_x);
    *y = view_axis(body.chunk.y, view->origin.y, body.body.y_fp, view->camera_offset_y);
    if (center) {
        *x += body.body.width * .5;
        *y += body.body.height * .5;
    }
    return true;
}

static int32_t pan_axis(double position, uint32_t extent) {
    double delta = position - extent / 2u;
    double distance = fabs(delta) - CAMERA_DEAD_ZONE;
    if (distance <= 0.) return 0;
    return (int32_t)copysign(fmin(distance, CAMERA_MAX_PAN), delta);
}

void dc_player_view_follow(dc_level_view_t *view, const uint32_t *ids, uint32_t count) {
    double left = INFINITY, right = -INFINITY, top = INFINITY, bottom = -INFINITY;
    for (uint32_t n = 0; n < count; ++n) {
        double x, y;
        if (!body_position(view, ids[n], count > 1u, &x, &y)) return;
        left = fmin(left, x);
        right = fmax(right, x);
        top = fmin(top, y);
        bottom = fmax(bottom, y);
    }
    int32_t dx = pan_axis((left + right) * .5, view->gpu->view_width);
    int32_t dy = pan_axis((top + bottom) * .5, view->gpu->view_height);
    if (dx || dy) dc_level_view_pan_pixels(view, dx, dy);
}

bool dc_player_view_split(dc_level_view_t *view, const uint32_t *ids,
                          uint32_t count, char *err, uint32_t cap) {
    dc_presentation_focus_t crops[DC_PRESENTATION_VIEW_LIMIT];
    uint32_t grid_width = view->gpu->view_width;
    uint32_t crop_width = grid_width / count;
    for (uint32_t n = 0; n < count; ++n) {
        double x, y;
        if (!body_position(view, ids[n], true, &x, &y)) {
            if (err && cap) snprintf(err, cap, "Split screen needs every viewport player admitted");
            return false;
        }
        uint32_t left = (uint32_t)fmin(fmax(x - crop_width / 2u, 0.), grid_width - crop_width);
        crops[n] = (dc_presentation_focus_t){left, 0u, crop_width, view->gpu->view_height};
    }
    if (dc_gpu_set_split_presentation(view->gpu, count, crops)) return true;
    if (err && cap) snprintf(err, cap, "Player viewports do not fit the presentation window");
    return false;
}

static uint32_t stick_cell(uint32_t start, uint32_t size, float axis) {
    double cell = size / 2u + round(axis * STICK_AIM_REACH);
    return start + (uint32_t)fmin(fmax(cell, 0.), size - 1u);
}

bool dc_player_view_aim(dc_level_view_t *view, uint32_t viewport, bool controller,
                         const fr_input_state_t *input, fr_player_point_t *target) {
    dc_presentation_layout_t crop;
    if (!dc_gpu_presentation_view(view->gpu, viewport, &crop)) return false;
    uint32_t x, y;
    if (controller) {
        x = stick_cell(crop.x, crop.width, input->values[FR_INPUT_AIM_X]);
        y = stick_cell(crop.y, crop.height, input->values[FR_INPUT_AIM_Y]);
    } else {
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        if (mx < 0 || my < 0 || !dc_presentation_pick(crop, (uint32_t)mx, (uint32_t)my, &x, &y))
            return false;
    }
    uint32_t lx, ly;
    dc_cell_address((int64_t)x + view->camera_offset_x, (int64_t)y + view->camera_offset_y,
                    &target->chunk, &lx, &ly);
    target->chunk.x += view->origin.x;
    target->chunk.y += view->origin.y;
    target->x = (float)lx;
    target->y = (float)ly;
    return true;
}

void dc_player_view_reticle(dc_level_view_t *view, uint32_t viewport,
                             const fr_player_point_t *target) {
    dc_presentation_reticle_t aim = {0};
    if (target) {
        double x = view_axis(target->chunk.x, view->origin.x,
            (int32_t)(target->x * DC_FLUID_FULL), view->camera_offset_x);
        double y = view_axis(target->chunk.y, view->origin.y,
            (int32_t)(target->y * DC_FLUID_FULL), view->camera_offset_y);
        if (x >= 0. && x < view->gpu->view_width && y >= 0. && y < view->gpu->view_height)
            aim = (dc_presentation_reticle_t){(uint32_t)x, (uint32_t)y, true};
    }
    dc_gpu_set_presentation_reticle(view->gpu, viewport, aim);
}
