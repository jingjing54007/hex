#include "common.h"

#include "hexutil/basics/error.h"
#include "hexutil/basics/hexgrid.h"
#include "hexutil/basics/noise.h"

#include "hex/graphics/font.h"
#include "hex/graphics/graphics.h"
#include "hex/game/game.h"
#include "hex/game/movement/movement.h"
#include "hex/game/movement/pathfinding.h"
#include "hex/view/level_renderer.h"
#include "hex/view/level_window.h"
#include "hex/view/view.h"


#define TILE_WIDTH 48
#define TILE_HEIGHT 32
#define X_SPACING 32
#define Y_SPACING 32

#define SLOPE_WIDTH (TILE_WIDTH - X_SPACING)
#define SLOPE_HEIGHT (Y_SPACING/2)


static Point source(3,5);
static Point target(17,12);

static Resources resources;

static TileType::pointer open_tile(boost::make_shared<TileType>());
static TileType::pointer closed_tile(boost::make_shared<TileType>());

UnitStack::pointer party = boost::make_shared<UnitStack>(0, source, Faction::pointer());

class PathfindingView: public GameView {
public:
    PathfindingView(Game *game):
            GameView(game, NULL, &::resources, NULL), movement_model(&game->level), pathfinder(&game->level, &movement_model), brush_radius(2) {
        level_view.discovered.fill();
    }

    void reset() {
        for (int i = 0; i < level_view.tile_views.height; i++)
            for (int j = 0; j < level_view.tile_views.width; j++) {
                level_view.tile_views[i][j].phase = 0;
            }

        pathfinder.clear();
        if (level_view.level->contains(source) && level_view.level->contains(target)) {
            pathfinder.start(*party, source, target);
        }
    }

    void left_click() {
        source = level_view.highlight_tile;
        reset();
    }

    void middle_click() {
        target = level_view.highlight_tile;
        reset();
    }

    void right_click() {
        Point tile_pos = level_view.highlight_tile;
        if (!level_view.level->contains(tile_pos))
            return;

        TileType::pointer paint;
        if (level_view.level->tiles[tile_pos].type == closed_tile)
            paint = open_tile;
        else
            paint = closed_tile;

        int num_scanlines = 2 * brush_radius + 1;
        std::vector<int> scanlines(num_scanlines);
        get_circle(tile_pos, brush_radius, scanlines);
        for (int i = 0; i < num_scanlines; i++) {
            for (int j = -scanlines[i]; j <= scanlines[i]; j++) {
                Point point(tile_pos.x + j, tile_pos.y - brush_radius + i);
                if (level_view.level->contains(point))
                    level_view.level->tiles[point].type = paint;
            }
        }

        reset();
    }

    void update() {
        if (pathfinder.state == RUNNING) {
            pathfinder.step();
        } else if (pathfinder.state == FINISHED) {
            Path new_path;
            pathfinder.build_path(new_path);
            for (auto iter = new_path.begin(); iter != new_path.end(); iter++) {
                level_view.tile_views[iter->y][iter->x].phase = 1;
            }
        }
    }

    MovementModel movement_model;
    Pathfinder pathfinder;
    int brush_radius;
};


class PathfindingRenderer: public LevelRenderer {
public:
    PathfindingRenderer(Graphics *graphics, Level *level, PathfindingView *view):
            LevelRenderer(graphics, &::resources, level, view, NULL),
            cache1(graphics, TextFormat(SmallFont10, true, 250,50,50), 1000),
            cache2(graphics, TextFormat(SmallFont10, true, 100,200,50), 1000) { }
    void render_tile(int x, int y, Point tile_pos);

private:
    TextCache cache1;
    TextCache cache2;
};

void PathfindingRenderer::render_tile(int x, int y, Point tile_pos) {
    x -= TILE_WIDTH/2;
    y -= Y_SPACING/2;
    int p1 = SLOPE_WIDTH;
    int p2 = TILE_WIDTH - SLOPE_WIDTH;
    int p3 = TILE_WIDTH;
    int p4 = SLOPE_HEIGHT;
    int p5 = TILE_HEIGHT;
    SDL_Point points[7] = { {p1,0}, {p2,0}, {p3,p4}, {p2,p5}, {p1,p5}, {0,p4}, {p1,0} };
    for (int i = 0; i < 7; i++) {
        points[i].x += x;
        points[i].y += y;
    }
    if (tile_pos == view->level_view.highlight_tile) {
        graphics->draw_lines(255,255,255, points, 7);
    } else {
        graphics->draw_lines(100,100,200, points, 7);
    }

    Tile &tile = level->tiles[tile_pos];
    TileView &tile_view = view->level_view.tile_views[tile_pos];

    Pathfinder &pathfinder = (static_cast<PathfindingView *>(view))->pathfinder;
    PathfinderNode &node = pathfinder.nodes[tile_pos];

    if (tile.type == closed_tile) {
        SDL_Rect rect = { x + 16, y + 8, TILE_WIDTH - 32, TILE_HEIGHT - 16 };
        SDL_SetRenderDrawColor(graphics->renderer, 100,200,200, 255);
        SDL_RenderFillRect(graphics->renderer, &rect);
    } else if (tile_pos == source) {
        SDL_Rect rect = { x + 16, y + 8, TILE_WIDTH - 32, TILE_HEIGHT - 16 };
        SDL_SetRenderDrawColor(graphics->renderer, 100,200,100, 255);
        SDL_RenderFillRect(graphics->renderer, &rect);
    } else if (tile_pos == target) {
        SDL_Rect rect = { x + 16, y + 8, TILE_WIDTH - 32, TILE_HEIGHT - 16 };
        SDL_SetRenderDrawColor(graphics->renderer, 200,100,100, 255);
        SDL_RenderFillRect(graphics->renderer, &rect);
    } else if (tile_view.phase == 1) {
        SDL_Rect rect = { x + 16, y + 8, TILE_WIDTH - 32, TILE_HEIGHT - 16 };
        SDL_SetRenderDrawColor(graphics->renderer, 200,200,100, 255);
        SDL_RenderFillRect(graphics->renderer, &rect);
    } else {
        char buffer[50];
        snprintf(buffer, sizeof(buffer), "%d", node.cost);
        if (node.cost == INT_MAX) snprintf(buffer, sizeof(buffer), "-");
        cache1.write_text(std::string(buffer), x + TILE_WIDTH/2, y + 12);

        snprintf(buffer, sizeof(buffer), "%d", node.heuristic);
        if (node.heuristic == INT_MIN) snprintf(buffer, sizeof(buffer), "-");
        cache2.write_text(std::string(buffer), x + TILE_WIDTH/2, y + TILE_HEIGHT - 12);
    }
}


void generate_level(Level &level) {
    PerlinNoise noise(5, 5);
    PerlinNoise noise2(10, 10);

    for (int i = 0; i < level.height; i++) {
        for (int j = 0; j < level.width; j++) {
            float px = (float) j / level.width;
            float py = (float) i / level.height;
            if (j % 2 == 1)
                py += 0.5f / level.height;

            float value = (noise.value(px, py) + noise2.value(py, px))/6.0f;
            if (value < 0.0f)
                level.tiles[i][j].type = closed_tile;
            else
                level.tiles[i][j].type = open_tile;
        }
    }
}


class BackgroundWindow: public UiWindow {
public:
    BackgroundWindow(UiLoop *loop, Graphics *graphics, PathfindingView *view):
        UiWindow(0, 0, graphics->width, graphics->height, WindowIsVisible|WindowIsActive|WindowWantsMouseEvents|WindowWantsKeyboardEvents),
        loop(loop), graphics(graphics), view(view) {
    }

    bool receive_mouse_event(SDL_Event *evt, int x, int y) {
        if (evt->type == SDL_MOUSEBUTTONUP && evt->button.button == SDL_BUTTON_LEFT) {
            if (!dragging) {
                view->left_click();
                return true;
            } else {
                dragging = false;
            }
        } else if (evt->type == SDL_MOUSEBUTTONUP && evt->button.button == SDL_BUTTON_MIDDLE) {
            view->middle_click();
            return true;
        } else if (evt->type == SDL_MOUSEBUTTONUP && evt->button.button == SDL_BUTTON_RIGHT) {
            view->right_click();
            return true;
        } else {
            if (evt->type == drag_event_type)
                dragging = true;

            return children[0]->receive_mouse_event(evt, x, y);
        }
        return false;
    }

    bool receive_keyboard_event(SDL_Event *evt) {
        if (evt->type == SDL_QUIT
            || (evt->type == SDL_KEYDOWN && evt->key.keysym.sym == SDLK_ESCAPE)) {
            loop->running = false;
            return true;
        } else if (evt->type == SDL_KEYDOWN && evt->key.keysym.sym >= SDLK_1 && evt->key.keysym.sym <= SDLK_9) {
            switch (evt->key.keysym.sym) {
                case SDLK_1: view->brush_radius = 1; break;
                case SDLK_2: view->brush_radius = 2; break;
                case SDLK_3: view->brush_radius = 3; break;
                case SDLK_4: view->brush_radius = 4; break;
                case SDLK_5: view->brush_radius = 5; break;
                case SDLK_6: view->brush_radius = 6; break;
                case SDLK_7: view->brush_radius = 7; break;
                case SDLK_8: view->brush_radius = 8; break;
                case SDLK_9: view->brush_radius = 9; break;
            }
            return true;
        }
        return false;
    }

    void draw(const UiContext& context) {
        view->update();

        SDL_SetRenderDrawColor(graphics->renderer, 0,0,0, 255);
        SDL_RenderClear(graphics->renderer);
    }

private:
    UiLoop *loop;
    Graphics *graphics;
    PathfindingView *view;
    bool dragging;
};


class TopWindow: public UiWindow {
public:
    TopWindow(Graphics *graphics):
        UiWindow(0, 0, 0, 0, WindowIsVisible),
        graphics(graphics) {
    }

    void draw(const UiContext& context) {
        graphics->update();
    }

private:
    Graphics *graphics;
};


void run() {
    register_property_names();

    open_tile->properties[Walkable] = 1;

    UnitType::pointer type = boost::make_shared<UnitType>();
    type->properties[Walking] = 1;
    Unit::pointer unit = boost::make_shared<Unit>();
    unit->type = type;
    party->units.push_back(unit);

    Graphics graphics;
    graphics.start("Pathfinding test", 800, 600, false);

    Game game;
    game.level.resize(200, 200);
    generate_level(game.level);

    PathfindingView view(&game);

    PathfindingRenderer level_renderer(&graphics, &game.level, &view);
    LevelWindow *level_window = new LevelWindow(graphics.width, graphics.height, &view, &level_renderer, NULL, &resources);

    UiLoop loop(&graphics, 25);
    BackgroundWindow *bg_window = new BackgroundWindow(&loop, &graphics, &view);
    loop.set_root_window(bg_window);
    bg_window->add_child(level_window);
    level_window->clear_flag(WindowIsActive);
    TopWindow *tw = new TopWindow(&graphics);
    bg_window->add_child(tw);
    loop.run();

    graphics.stop();
}

int main(int argc, char *argv[]) {
    try {
        run();
    } catch (Error &ex) {
        BOOST_LOG_TRIVIAL(fatal) << "Failed with: " << ex.what();
    }

    return 0;
}
