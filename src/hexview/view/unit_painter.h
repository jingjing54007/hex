#ifndef UNIT_PAINTER_H
#define UNIT_PAINTER_H

#include "hexutil/messaging/counter.h"


namespace hex {

class Game;
class GameView;
class Resources;
class UnitView;
class UnitStackView;

class UnitPainter {
public:
    UnitPainter(Resources *resources):
            resources(resources),
            unit_paint_counter("paint.unit"), unit_paint_time("paint.unit.time"), script_error_counter("paint.unit.error") { }

    void repaint(UnitView& unit_view, Unit& unit);
    void repaint(UnitStackView& unit_stack_view, UnitStack& unit_stack);

private:
    Resources *resources;

    Counter unit_paint_counter;
    Counter unit_paint_time;
    Counter script_error_counter;
};

};

#endif
