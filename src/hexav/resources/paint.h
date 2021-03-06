#ifndef PAINT_H
#define PAINT_H

#include "hexutil/basics/point.h"
#include "hexutil/scripting/scripting.h"

#include "hexav/graphics/graphics.h"


namespace hex {

class Resources;

#define FRAME_RATE_BASE 1048576

inline int frame_ms_to_rate(int ms) {
    return FRAME_RATE_BASE / ms;
}

inline int frame_rate_to_ms(int rate) {
    return FRAME_RATE_BASE / rate;
}


class PaintItem {
public:
    int get_duration() const;
    SDL_Rect get_bounds() const;

public:
    int offset_x, offset_y;
    int tile_width, tile_height;
    int clip_x, clip_y;
    std::vector<Image *> frames;
    int frame_rate;
    int blend_alpha;
    int blend_addition;
    std::string text;
};


class Paint {
public:
    Paint();

    void render(int x, int y, int phase, Graphics *graphics);

    void clear();
    void add(const PaintItem& item);

    int get_duration() const {
        return duration;
    }
    const SDL_Rect& get_bounds() const {
        return bounds;
    }

private:
    std::vector<PaintItem> items;
    int duration;
    SDL_Rect bounds;
};

class PaintExecution: public Execution {
public:
    PaintExecution(AtomMap<Script> *scripts, Resources *resources, Paint *paint): Execution(scripts), resources(resources), paint(paint) {
        paint_library_atom = AtomRegistry::get_instance().atom("paint_library");
        paint_offset_x_atom = AtomRegistry::get_instance().atom("paint_offset_x");
        paint_offset_y_atom = AtomRegistry::get_instance().atom("paint_offset_y");
        paint_tile_width_atom = AtomRegistry::get_instance().atom("paint_tile_width");
        paint_tile_height_atom = AtomRegistry::get_instance().atom("paint_tile_height");
        paint_clip_x_atom = AtomRegistry::get_instance().atom("paint_clip_x");
        paint_clip_y_atom = AtomRegistry::get_instance().atom("paint_clip_y");
        paint_blend_alpha_atom = AtomRegistry::get_instance().atom("paint_blend_alpha");
        paint_blend_addition_atom = AtomRegistry::get_instance().atom("paint_blend_addition");
        paint_frame_offset_atom = AtomRegistry::get_instance().atom("paint_frame_offset");
    }

    int frame_width(int frame_num);
    int frame_height(int frame_num);
    void paint_frame(int frame_num);
    void paint_frame(Atom image_libary, int frame_num, int offset_x, int offset_y, int tile_width, int tile_height, int clip_x, int clip_y, int blend_alpha, int blend_addition);
    void paint_animation(Atom image_libary, int frame_rate, const std::vector<int>& frame_nums, int offset_x, int offset_y, int blend_alpha, int blend_addition);
    void paint_text(const std::string& text, int offset_x, int offset_y);

    void run(Script *script);

public:
    Resources *resources;
    Paint *paint;

    Atom paint_library_atom;
    Atom paint_offset_x_atom;
    Atom paint_offset_y_atom;
    Atom paint_tile_width_atom;
    Atom paint_tile_height_atom;
    Atom paint_clip_x_atom;
    Atom paint_clip_y_atom;
    Atom paint_blend_alpha_atom;
    Atom paint_blend_addition_atom;
    Atom paint_frame_offset_atom;
};

void register_paint_interpreters();

};

#endif
