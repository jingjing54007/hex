#include "common.h"

#include "hexutil/basics/point.h"
#include "hexutil/basics/hexgrid.h"

#include "hexgame/game/game.h"

#include "hexview/resources/paint.h"
#include "hexview/resources/resources.h"
#include "hexview/view/view.h"


void Paint::render(int x, int y, int phase, Graphics *graphics) {
    for (auto iter = items.begin(); iter != items.end(); iter++) {
        PaintItem& item = *iter;
        if (item.frames.size() == 0)
            continue;

        Image *frame = item.frames[0];
        if (item.frame_rate != -1) {
            int pos = (phase * item.frame_rate / FRAME_RATE_BASE);
            if (pos >= item.frames.size())
                pos %= item.frames.size();
            frame = item.frames[pos];
        }
        if (!frame)
            continue;

        if (item.blend_addition > 0) {
            graphics->blit(frame, x + item.offset_x, y + item.offset_y, SDL_BLENDMODE_ADD, item.blend_addition);
        } else if (item.blend_addition < 0) {
            graphics->blit(frame, x + item.offset_x, y + item.offset_y, SDL_BLENDMODE_MOD);
        } else {
            if (item.blend_alpha != 0) {
                graphics->blit(frame, x + item.offset_x, y + item.offset_y, SDL_BLENDMODE_BLEND, item.blend_alpha);
            } else {
                graphics->blit(frame, x + item.offset_x, y + item.offset_y, SDL_BLENDMODE_BLEND);
            }
        }
    }
}

void Paint::clear() {
    items.clear();
    duration = 0;
}

void Paint::add(const PaintItem& item) {
    items.push_back(item);
    if (item.frame_rate != -1) {
        int item_duration = item.get_duration();
        if (item_duration > duration) {
            duration = item_duration;
        }
    }
}


void PaintExecution::paint_frame(int frame_num) {
    Atom paint_library = get(paint_library_atom).get_as_atom();
    int offset_x = get_as<int>(paint_offset_x_atom);
    int offset_y = get_as<int>(paint_offset_y_atom);
    int blend_alpha = get_as<int>(paint_blend_alpha_atom);
    int blend_addition = get_as<int>(paint_blend_addition_atom);
    int frame_offset = get_as<int>(paint_frame_offset_atom);
    paint_frame(paint_library, frame_offset + frame_num, offset_x, offset_y, blend_alpha, blend_addition);
}

void PaintExecution::paint_frame(Atom image_library, int frame_num, int offset_x, int offset_y, int blend_alpha, int blend_addition) {
    PaintItem pi;
    pi.offset_x = offset_x;
    pi.offset_y = offset_y;
    pi.frame_rate = -1;
    pi.blend_alpha = blend_alpha;
    pi.blend_addition = blend_addition;
    Image *image = resources->get_library_image(image_library, frame_num);
    pi.frames.push_back(image);
    paint->add(pi);
}

void PaintExecution::paint_animation(Atom image_library, int frame_time, const std::vector<int>& frame_nums, int offset_x, int offset_y, int blend_alpha, int blend_addition) {
    PaintItem pi;
    pi.offset_x = offset_x;
    pi.offset_y = offset_y;
    pi.frame_rate = frame_time;
    pi.blend_alpha = blend_alpha;
    pi.blend_addition = blend_addition;
    for (auto iter = frame_nums.begin(); iter != frame_nums.end(); iter++) {
        Image *image = resources->get_library_image(image_library, *iter);
        pi.frames.push_back(image);
    }
    paint->add(pi);
}

void PaintExecution::run(Script *script) {
    variables.set<int>(paint_offset_x_atom, 0);
    variables.set<int>(paint_offset_y_atom, 0);
    variables.set<int>(paint_blend_addition_atom, 0);
    variables.set<int>(paint_blend_alpha_atom, 0);
    variables.set<int>(paint_frame_offset_atom, 0);
    Execution::run(script);
}


bool TransitionPaintExecution::apply_transition(const std::vector<int>& dirs, const std::vector<int>& frame_nums) {

    for (auto dir_iter = dirs.begin(); dir_iter != dirs.end(); dir_iter++) {
        Point neighbour_pos;
        get_neighbour(tile_pos, *dir_iter, &neighbour_pos);
        if (!game->level.contains(neighbour_pos)) {
            return false;
        }
        TileViewDef::pointer neighbour_def = view->level_view.tile_views[neighbour_pos].view_def;
        if (!neighbour_def) {
            return false;
        }

        if (!boost::regex_match(neighbour_def->name, pattern_re)) {
            return false;
        }
    }

    if (frame_nums.empty())
        return false;

    int variation = get("tile_variation");
    int r = variation % frame_nums.size();
    paint_frame(frame_nums[r]);

    return true;
}
