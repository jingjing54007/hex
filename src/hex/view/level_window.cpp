#include "common.h"

#include "hex/basics/point.h"
#include "hex/game/game.h"
#include "hex/game/game_serialisation.h"
#include "hex/resources/resources.h"
#include "hex/view/view.h"
#include "hex/view/level_renderer.h"
#include "hex/view/level_window.h"

#define TILE_WIDTH 48
#define X_SPACING 32
#define Y_SPACING 32

#define SLOPE_WIDTH (TILE_WIDTH - X_SPACING)
#define SLOPE_HEIGHT (Y_SPACING/2)

LevelWindow::LevelWindow(int width, int height, LevelView *level_view, LevelRenderer *level_renderer, Resources *resources):
        UiWindow(0, 0, width, height),
        level_view(level_view), level_renderer(level_renderer), resources(resources),
        shift_x(SLOPE_WIDTH), shift_y(SLOPE_HEIGHT), x_spacing(X_SPACING), y_spacing(Y_SPACING) {
}

LevelWindow::~LevelWindow() {
}

void LevelWindow::set_mouse_position(int x, int y) {
    Point tile_pos;

    mouse_to_tile(x, y, &tile_pos);
    level_view->set_highlight_tile(tile_pos);
}

void LevelWindow::mouse_to_tile(int x, int y, Point *tile) {
    x += shift_x;
    y += shift_y;

    int x_mod = x % (2*X_SPACING);
    int x_div = x / (2*X_SPACING);

    int segment = -99;
    if (x_mod < SLOPE_WIDTH)
        segment = 0;
    else if (x_mod < X_SPACING)
        segment = 1;
    else if (x_mod < TILE_WIDTH)
        segment = 2;
    else
        segment = 3;

    if (segment == 0) {
        int y_mod = y % Y_SPACING;
        int y_div = y / Y_SPACING;
        if (y_mod*SLOPE_WIDTH < SLOPE_HEIGHT - x_mod*SLOPE_HEIGHT) {
            tile->x = 2*x_div - 1;
            tile->y = y_div - 1;
        } else if (y_mod*SLOPE_WIDTH < SLOPE_HEIGHT*SLOPE_WIDTH + x_mod*SLOPE_HEIGHT) {
            tile->x = 2*x_div;
            tile->y = y_div;
        } else {
            tile->x = 2*x_div - 1;
            tile->y = y_div;
        }
    } else if (segment == 1) {
        tile->x = 2 * x_div;
        tile->y = y / Y_SPACING;
    } else if (segment == 2) {
        x_mod -= X_SPACING;
        int y_mod = y % Y_SPACING;
        int y_div = y / Y_SPACING;
        if (y_mod*SLOPE_WIDTH < x_mod*SLOPE_HEIGHT) {
            tile->x = 2*x_div + 1;
            tile->y = y_div - 1;
        } else if (y_mod*SLOPE_WIDTH < 2*SLOPE_HEIGHT*SLOPE_WIDTH - x_mod*SLOPE_HEIGHT) {
            tile->x = 2*x_div;
            tile->y = y_div;
        } else {
            tile->x = 2*x_div + 1;
            tile->y = y_div;
        }
    } else if (segment == 3) {
        tile->x = 2 * x_div + 1;
        tile->y = (y - SLOPE_HEIGHT) / Y_SPACING;
    }
}

void LevelWindow::tile_to_pixel(const Point tile, int *px, int *py) {
    *px = tile.x * x_spacing - shift_x;
    *py = tile.y * y_spacing - shift_y;
    if (tile.x % 2 == 1)
        *py += y_spacing / 2;
}

void LevelWindow::shift(int xrel, int yrel) {
    int min_shift_x = SLOPE_WIDTH;
    int max_shift_x = level_view->tile_views.width * X_SPACING - width;
    int min_shift_y = SLOPE_HEIGHT;
    int max_shift_y = level_view->tile_views.height * Y_SPACING - height;

    shift_x -= xrel;
    if (shift_x < min_shift_x)
        shift_x = min_shift_x;
    if (shift_x > max_shift_x)
        shift_x = max_shift_x;

    shift_y -= yrel;
    if (shift_y < min_shift_y)
        shift_y = min_shift_y;
    if (shift_y > max_shift_y)
        shift_y = max_shift_y;
}

void LevelWindow::left_click(int x, int y) {
    Point tile_pos;

    mouse_to_tile(x, y, &tile_pos);
    level_view->left_click_tile(tile_pos);
}

void LevelWindow::right_click(int x, int y) {
    Point tile_pos;

    mouse_to_tile(x, y, &tile_pos);
    level_view->right_click_tile(tile_pos);
}

void LevelWindow::draw() {
    level_renderer->clear(x, y, width, height);
    draw_level(&LevelRenderer::render_tile);
    draw_level(&LevelRenderer::render_unit_stack);
    draw_level(&LevelRenderer::render_path_arrow);
    draw_moving_unit();
}

void LevelWindow::draw_level(LevelRenderer::RenderMethod render) {
    Point min_pos, max_pos;
    mouse_to_tile(0, 0, &min_pos);
    mouse_to_tile(width, height, &max_pos);

    min_pos.x -= 2;
    if (min_pos.x % 2 == 1)
        min_pos.x++;
    if (min_pos.x < 0)
        min_pos.x = 0;
    min_pos.y--;
    if (min_pos.y < 0)
        min_pos.y = 0;
    max_pos.x += 2;
    if (max_pos.x > level_view->tile_views.width)
        max_pos.x = level_view->tile_views.width;
    max_pos.y += 2;
    if (max_pos.y > level_view->tile_views.height)
        max_pos.y = level_view->tile_views.height;

    for (int i = min_pos.y; i < max_pos.y; i++) {
        for (int j = min_pos.x; j < max_pos.x; j += 2) {
            int xpos = j*x_spacing - shift_x;
            int ypos = i*y_spacing - shift_y;
            Point tile_pos(j, i);
            if (!level_view->discovered.check(tile_pos))
                continue;
            (*level_renderer.*render)(xpos, ypos, tile_pos);
        }

        for (int j = min_pos.x + 1; j < max_pos.x; j += 2) {
            int y_offset = y_spacing / 2;
            int xpos = j*x_spacing - shift_x;
            int ypos = i*y_spacing + y_offset - shift_y;
            Point tile_pos(j, i);
            if (!level_view->discovered.check(tile_pos))
                continue;
            (*level_renderer.*render)(xpos, ypos, tile_pos);
        }
    }
}

void LevelWindow::draw_moving_unit() {
    if (level_view->moving_unit == NULL)
        return;

    UnitStackView *stack_view = level_view->get_unit_stack_view(*level_view->moving_unit);

    int step = level_view->moving_unit_progress / 1000;
    Point prev_pos = level_view->moving_unit_path[step];
    Point next_pos = level_view->moving_unit_path[step + 1];

    if (!level_view->visibility.check(prev_pos) && !level_view->visibility.check(next_pos))
        return;

    int f = level_view->moving_unit_progress % 1000;

    int px1, py1, px2, py2;

    tile_to_pixel(prev_pos, &px1, &py1);
    tile_to_pixel(next_pos, &px2, &py2);

    int px = (px1 * (1000 - f) + px2 * f) / 1000;
    int py = (py1 * (1000 - f) + py2 * f) / 1000;
    bool selected = stack_view->selected;
    stack_view->selected = false;
    level_renderer->draw_unit_stack(px, py, *stack_view);
    stack_view->selected = selected;
}
