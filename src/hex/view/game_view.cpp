#include "common.h"

#include "hex/basics/error.h"
#include "hex/basics/hexgrid.h"
#include "hex/game/game.h"
#include "hex/game/game_messages.h"
#include "hex/game/movement/movement.h"
#include "hex/game/movement/pathfinding.h"
#include "hex/messaging/message.h"
#include "hex/resources/view_def.h"
#include "hex/view/player.h"
#include "hex/view/view.h"


Ghost::Ghost(UnitStack *target, Path path): target(target), path(path), progress(0) {
}


GameView::GameView(Game *game, Player *player, Resources *resources, MessageReceiver *dispatcher):
        game(game), player(player), level_view(&game->level), resources(resources), dispatcher(dispatcher),
        last_update(0), phase(0), selected_stack_id(0), selected_structure(NULL), debug_mode(false) {
}

// Assumes 1000 increments between frames
#define frame_incr(bpm, update_ms) ((bpm) * (update_ms) / 60)

void GameView::update() {
    unsigned int ticks = SDL_GetTicks();
    if (ticks < last_update + 25)
        return;

    unsigned int update_ms = ticks - last_update;

    int level_bpm = 60 * 40;
    phase += frame_incr(level_bpm, update_ms);

    last_update = ticks;
    for (int i = 0; i < level_view.level->height; i++)
        for (int j = 0; j < level_view.level->width; j++) {
            TileViewDef *view_def = level_view.tile_views[i][j].view_def;
            if (view_def ==  NULL)
                continue;
            level_view.tile_views[i][j].phase += frame_incr(view_def->animation.bpm, update_ms);
        }

    for (std::map<int, UnitStackView>::iterator iter = unit_stack_views.begin(); iter != unit_stack_views.end(); iter++) {
        if (iter->second.stack == NULL || iter->second.moving)
            continue;
        UnitViewDef *view_def = iter->second.view_def;
        if (view_def == NULL)
            continue;
        iter->second.phase += frame_incr(view_def->hold_animations[iter->second.facing].bpm, update_ms);
    }

    level_view.ghost_visibility.clear();
    std::vector<Ghost>::iterator iter = ghosts.begin();
    while (iter != ghosts.end()) {
        Ghost& ghost = *iter;

        UnitStackView *target_view = get_stack_view(ghost.target->id);
        UnitViewDef *view_def = target_view->view_def;
        ghost.progress += frame_incr(view_def->move_speed, update_ms);
        unsigned int step = ghost.progress / 1000;
        if (step >= ghost.path.size() - 1) {
            target_view->locked = false;
            target_view->moving = false;
            if (player->has_view(ghost.target->owner)) {
                level_view.visibility.unmask(ghost.target);
                update_visibility();
            }
            iter = ghosts.erase(iter);
        } else {
            Point &prev_pos = ghost.path[step];
            Point &next_pos = ghost.path[step + 1];
            ghost.position = next_pos;
            target_view->facing = get_direction(next_pos, prev_pos);
            target_view->phase += frame_incr(view_def->move_animations[target_view->facing].bpm, update_ms);
            if (player->has_view(ghost.target->owner)) {
                level_view.discovered.draw(next_pos, ghost.target->sight(), true);
                level_view.ghost_visibility.draw(next_pos, ghost.target->sight(), true);
            }
            iter++;
        }
    }
}

void GameView::left_click_tile(const Point& tile_pos) {
    Tile& tile = level_view.level->tiles[tile_pos];
    if (tile.stack != NULL && tile.stack->id != selected_stack_id) {
        if (selected_stack_id) {
            UnitStackView *current_stack_view = get_stack_view(selected_stack_id);
            current_stack_view->selected = false;
        }
        UnitStack *stack = tile.stack;
        UnitStackView *stack_view = get_stack_view(stack->id);
        stack_view->selected = true;
        selected_stack_id = stack->id;
        selected_units.clear();
        for (unsigned int i = 0; i < stack->units.size(); i++) {
            selected_units.insert(i);
        }
        if (selected_structure != NULL) {
            StructureView *structure_view = level_view.tile_views[selected_structure->position].structure_view;
            structure_view->selected = false;
        }
        selected_structure = NULL;

        if (player->has_view(stack->owner))
            set_drawn_path(stack_view->path);
        else
            set_drawn_path(Path());
    } else if (tile.structure != NULL) {
        if (selected_stack_id) {
            UnitStackView *stack_view = get_stack_view(selected_stack_id);
            stack_view->selected = false;
        }
        if (selected_structure != NULL) {
            StructureView *structure_view = level_view.tile_views[selected_structure->position].structure_view;
            structure_view->selected = false;
        }
        selected_stack_id = 0;
        selected_structure = tile.structure;
        set_drawn_path(Path());
        StructureView *structure_view = level_view.tile_views[selected_structure->position].structure_view;
        structure_view->selected = true;
    }
}

void GameView::right_click_tile(const Point& tile_pos) {
    UnitStack *current_stack = game->get_stack(selected_stack_id);
    UnitStackView *current_stack_view = get_stack_view(selected_stack_id);
    if (current_stack == NULL || current_stack_view == NULL || current_stack_view->locked)
        return;

    if (level_view.level->contains(tile_pos) && player->has_control(current_stack->owner)) {
        MovementModel movement_model(level_view.level);
        Pathfinder pathfinder(level_view.level, &movement_model);
        pathfinder.start(current_stack, current_stack->position, tile_pos);
        pathfinder.complete();
        Path new_path;
        pathfinder.build_path(new_path);

        UnitStackView *stack_view = get_stack_view(selected_stack_id);

        if (stack_view->path == new_path) {
            int target_id = 0;
            UnitStack *target_stack = level_view.level->tiles[new_path.back()].stack;
            if (target_stack != NULL) {
                if (target_stack->owner == current_stack->owner) {
                    if (target_stack->units.size() + current_stack->units.size() > MAX_UNITS)
                        return;
                }
                target_id = target_stack->id;
            }
            dispatcher->receive(create_message(UnitMove, current_stack->id, selected_units, new_path, target_id));
        } else {
            stack_view->path = new_path;
            set_drawn_path(stack_view->path);
        }
    }
}

void GameView::set_drawn_path(const Path& path) {
    // Erase existing path
    for (std::vector<Point>::const_iterator iter = drawn_path.begin(); iter != drawn_path.end(); iter++) {
        level_view.tile_views[*iter].path_dir = -1;
    }

    drawn_path = path;

    // Draw new path
    Point last(-1,-1);
    for (std::vector<Point>::const_iterator iter = drawn_path.begin(); iter != drawn_path.end(); iter++) {
        if (last.x != -1)
            level_view.tile_views[*iter].path_dir = get_direction(last, *iter);
        last = *iter;
    }
}

void GameView::update_visibility() {
    level_view.visibility.clear();
    for (std::map<int, UnitStackView>::iterator iter = unit_stack_views.begin(); iter != unit_stack_views.end(); iter++) {
        UnitStack *stack = iter->second.stack;
        if (player->has_view(stack->owner) && !iter->second.moving) {
            level_view.discovered.draw(stack->position, stack->sight(), true);
            level_view.visibility.draw(stack->position, stack->sight(), true);
        }
    }

    for (int i = 0; i < level_view.level->height; i++) {
        for (int j = 0; j < level_view.level->width; j++) {
            Point tile_pos(j, i);
            TileView& tile_view = level_view.tile_views[tile_pos];
            if (tile_view.structure_view == NULL)
                continue;
            Structure *structure = tile_view.structure_view->structure;
            if (player->has_view(structure->owner)) {
                level_view.discovered.draw(tile_pos, structure->sight(), true);
                level_view.visibility.draw(tile_pos, structure->sight(), true);
            }
        }
    }
}

UnitStackView *GameView::get_stack_view(int stack_id) {
    std::map<int, UnitStackView>::iterator iter = unit_stack_views.find(stack_id);
    if (iter == unit_stack_views.end())
        return NULL;

    return &iter->second;
}

TileView *GameView::get_tile_view(const Point tile_pos) {
    if (level_view.level->contains(tile_pos)) {
        return &level_view.tile_views[tile_pos];
    }

    return NULL;
}

    void GameView::transfer_units(int stack_id, const IntSet selected_units, Path path, int target_id) {
    /* By the time we get here, the units have already moved in the game state. */

    UnitStack *stack = game->get_stack(stack_id);
    if (stack == NULL) {
        BOOST_LOG_TRIVIAL(warning) << "No stack with id " << stack_id;
        return;
    }
    UnitStack *target_stack = game->get_stack(target_id);
    if (target_stack == NULL) {
        BOOST_LOG_TRIVIAL(warning) << "No stack with id " << target_id;
        return;
    }

    if (path.size() < 2)
        return;

    bool new_target = selected_units.size() == target_stack->units.size();

    UnitStackView *stack_view = &unit_stack_views[stack->id];
    stack_view->path = Path();
    if (selected_stack_id == stack->id) {
        set_drawn_path(stack_view->path);
    }

    UnitStackView *target_view = &unit_stack_views[target_stack->id];
    target_view->locked = true;
    if (new_target)
        target_view->moving = true;
    Ghost ghost(target_stack, path);
    ghosts.push_back(ghost);
    if (player->has_view(ghost.target->owner)) {
        update_visibility();
    }
}

void GameView::mark_ready() {
    for (std::map<int, FactionView *>::const_iterator iter = faction_views.begin(); iter != faction_views.end(); iter++) {
        int faction_id = iter->first;
        FactionView *faction_view =iter->second;
        if (player->has_control(faction_view->faction)) {
            dispatcher->receive(create_message(FactionReady, faction_id, true));
        }
    }
}
