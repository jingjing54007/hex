#include "common.h"

#include "hexav/graphics/font.h"
#include "hexav/graphics/graphics.h"

#include "hexgame/game/game.h"

#include "hexview/view/status_window.h"
#include "hexview/view/view.h"


namespace hex {

StatusWindow::StatusWindow(int x, int y, int width, int height, Graphics *graphics, GameView *view):
        UiWindow(x, y, width, height, WindowIsVisible), graphics(graphics), view(view) {
}

void StatusWindow::draw_faction_readiness() {
    int x_offset = x + width - 100;
    for (auto iter = view->faction_views.begin(); iter != view->faction_views.end(); iter++) {
        Faction::pointer faction = iter->second->faction;
        FactionViewDef::pointer view_def = iter->second->view_def;
        graphics->fill_rectangle(view_def->r, view_def->g, view_def->b, x_offset, y+2, 8, height-4);
        if (faction->ready) {
            graphics->draw_rectangle(255,255,255, x_offset, y+2, 8, height-4);
        }
        x_offset += 8;
    }
}

void StatusWindow::draw(const UiContext& context) {
    graphics->fill_rectangle(50,50,100, x, y, width, height);

    draw_faction_readiness();

    if (view->debug_mode) {
        TextFormat tf(SmallFont10, false, 255,255,255);
        tf.write_text(graphics, "Debug", x + width - 42, y + 4);
    }
}

};
