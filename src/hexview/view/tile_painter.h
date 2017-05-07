#ifndef TILE_PAINTER_H
#define TILE_PAINTER_H

#include "hexutil/basics/point.h"

class Game;
class GameView;
class Resources;
class TileView;

class TilePainter {
public:
    TilePainter(Game *game, GameView *view, Resources *resources);

    void repaint(Point offset, int len);

    void repaint_tile(TileView& tile_view, const Tile& tile, const Point tile_pos);
    void repaint_transition(TileView& tile_view, const Tile& tile, const Point tile_pos);
    void repaint_feature(TileView& tile_view, const Tile& tile, const Point tile_pos);

private:
    Game *game;
    GameView *view;
    Resources *resources;

    Counter tile_paint_counter;
    Counter tile_script_error_counter;
    Counter feature_paint_counter;
    Counter feature_script_error_counter;
    Counter transition_paint_counter;
    Counter transition_script_error_counter;
};

#endif
