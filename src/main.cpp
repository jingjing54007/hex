#include "common.h"

#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include "hexutil/basics/error.h"
#include "hexutil/basics/statistics.h"
#include "hexutil/messaging/message.h"
#include "hexutil/messaging/serialiser.h"
#include "hexutil/messaging/writer.h"
#include "hexutil/messaging/receiver.h"
#include "hexutil/messaging/publisher.h"
#include "hexutil/messaging/logger.h"
#include "hexutil/messaging/queue.h"
#include "hexutil/networking/networking.h"
#include "hexutil/messaging/builtin_messages.h"

#include "hexav/audio/audio.h"
#include "hexav/graphics/graphics.h"
#include "hexav/resources/resource_messages.h"
#include "hexav/ui/window_painter.h"

#include "hexgame/ai/ai.h"
#include "hexgame/game/game.h"
#include "hexgame/game/game_arbiter.h"
#include "hexgame/game/game_messages.h"
#include "hexgame/game/game_updater.h"
#include "hexgame/game/game_writer.h"
#include "hexgame/game/generation/generator.h"
#include "hexgame/node/node.h"

#include "hexview/chat/chat.h"
#include "hexview/editor/palette.h"
#include "hexview/editor/editor.h"
#include "hexview/view/audio_renderer.h"
#include "hexview/view/ghost.h"
#include "hexview/view/level_renderer.h"
#include "hexview/view/level_window.h"
#include "hexview/view/map_window.h"
#include "hexview/view/message_window.h"
#include "hexview/view/stack_window.h"
#include "hexview/view/status_window.h"
#include "hexview/view/player.h"
#include "hexview/view/pre_updater.h"
#include "hexview/view/transition_paint.h"
#include "hexview/view/unit_info_window.h"
#include "hexview/view/unit_renderer.h"
#include "hexview/view/view.h"
#include "hexview/view/view_resource_loader.h"
#include "hexview/view/view_resource_messages.h"
#include "hexview/view/view_updater.h"
#include "hexview/view/combat/combat_screen.h"


namespace hex {

struct Options {
    bool server_mode;
    bool client_mode;
    std::string host_name;
    std::string host_addr;
    std::string load_filename;
    int width, height;
    bool fullscreen;
};

void load_resources(ViewResources *resources, Graphics *graphics, Audio *audio) {
    ImageLoader image_loader(resources, graphics);
    SoundLoader sound_loader(resources, audio);
    ViewResourceLoader loader(resources, &image_loader, &sound_loader);
    loader.load(std::string("data/resources.txt"));
    resources->resolve_references();
}

void load_game(const std::string& filename, MessageReceiver& updater) {
    replay_messages(filename, updater);
}

void save_game(const std::string& filename, Game *game) {
    std::ofstream f(filename.c_str());
    MessageWriter message_writer(f);
    GameWriter game_writer(&message_writer);
    game_writer.write(game);
}

NodeInterface *make_node_interface(Options& options) {
    NodeInterface *node;

    if (options.client_mode) {
        node = new ClientNode(options.host_addr);
    } else if (options.server_mode) {
        node = new ServerNode();
    } else {
        node = new LocalNode();
    }

    return node;
}

class BackgroundWindow: public UiWindow {
public:
    BackgroundWindow(UiLoop *loop, Generator *generator, Game *game, GameView *game_view,
            NodeInterface *node_interface,
            LevelRenderer *level_renderer, PaletteWindow *palette_window):
        UiWindow(0, 0, 0, 0, WindowIsVisible|WindowIsActive|WindowWantsKeyboardEvents),
        loop(loop), generator(generator), game(game), game_view(game_view),
        node_interface(node_interface), level_renderer(level_renderer), palette_window(palette_window) { }

    bool receive_keyboard_event(SDL_Event *evt) {
        if (evt->type == SDL_QUIT
            || (evt->type == SDL_KEYDOWN && evt->key.keysym.sym == SDLK_ESCAPE)) {
            loop->running = false;
            return true;
        }

        if (evt->type == SDL_KEYDOWN && evt->key.keysym.sym == SDLK_F2) {
            save_game("save.txt", game);
            return true;
        } else if (evt->type == SDL_KEYDOWN && evt->key.keysym.sym == SDLK_F4) {
            palette_window->toggle();
            return true;
        } else if (evt->type == SDL_KEYDOWN && evt->key.keysym.sym == SDLK_F5) {
            game_view->debug_mode = !game_view->debug_mode;
            level_renderer->show_hexagons = game_view->debug_mode;
            return true;
        } else if (evt->type == SDL_KEYDOWN && evt->key.keysym.sym == SDLK_F6) {
            LocalNode *local = dynamic_cast<LocalNode *>(node_interface);
            if (local != NULL) {
                MessageReceiver& emitter = local->get_emitter();

                generator->mountain_level += (evt->key.keysym.mod & KMOD_SHIFT) ? 0.2f : -0.2f;
                std::cerr << "mountain_level = " << generator->mountain_level << std::endl;
                generator->create_game(emitter);
                emitter.receive(create_message(GrantFactionView, 0, 2, true));
                emitter.receive(create_message(GrantFactionControl, 0, 2, true));
                emitter.receive(create_message(GrantFactionControl, 0, 3, true));
            }
            return true;
        }

        if (evt->type == SDL_KEYDOWN && evt->key.keysym.sym == SDLK_F12) {
            game_view->mark_ready();
        }

        return false;
    }

    void draw(const UiContext& context) {
        game_view->update();
        node_interface->update();
    }

private:
    UiLoop *loop;
    Generator *generator;
    Game *game;
    GameView *game_view;
    NodeInterface *node_interface;
    LevelRenderer *level_renderer;
    PaletteWindow *palette_window;
};

class TopWindow: public UiWindow {
public:
    TopWindow(Graphics *graphics, Audio *audio):
            UiWindow(0, 0, 0, 0, WindowIsVisible),
            graphics(graphics), audio(audio) { }

    void draw(const UiContext& context) {
        graphics->update();
        audio->update();
    }

private:
    Graphics *graphics;
    Audio *audio;
};

void run(Options& options) {
    register_builtin_messages();
    register_game_messages();
    register_resource_messages();
    register_view_resource_messages();
    register_property_names();

    register_builtin_interpreters();
    register_paint_interpreters();
    register_transition_paint_interpreters();
    register_window_interpreters();

    Graphics graphics("Hex", options.width, options.height, options.fullscreen);

    ViewResources resources;
    Audio audio(&resources);  // TODO audio should not depend on resources
    audio.start();

    load_resources(&resources, &graphics, &audio);

    NodeInterface *node_interface = make_node_interface(options);

    Player player(0, std::string("player"));

    Game game;
    GameView game_view(&game, &player, &resources, &node_interface->get_throttle(), node_interface);
    PreUpdater pre_updater(&game);
    node_interface->subscribe(&pre_updater);

    GameUpdater game_updater(&game);
    node_interface->subscribe(&game_updater);

    ViewUpdater view_updater(&game, &game_view, &resources);
    node_interface->subscribe(&view_updater);

    Generator generator;

    if (!options.client_mode) {
        LocalNode *local = static_cast<LocalNode *>(node_interface);
        local->add_ai("independent");

        MessageReceiver& emitter = local->get_emitter();
        if (options.load_filename.empty()) {
            generator.create_game(emitter);
        } else {
            load_game(options.load_filename, emitter);
        }

        emitter.receive(create_message(GrantFactionView, 0, 2, true));
        emitter.receive(create_message(GrantFactionControl, 0, 2, true));
        emitter.receive(create_message(GrantFactionControl, 0, 3, true));
    }

    node_interface->start();

    int sidebar_width = StackWindow::window_width;
    int sidebar_position = graphics.width - sidebar_width;
    int map_window_height = 200;
    int stack_window_height = StackWindow::window_height;
    int status_window_height = StatusWindow::window_height;
    int message_window_height = graphics.height - map_window_height - stack_window_height - status_window_height;
    int unit_info_window_x = (graphics.width - UnitInfoWindow::unit_info_window_width) / 2;
    int unit_info_window_y = (graphics.height - UnitInfoWindow::unit_info_window_height) / 2;

    UnitRenderer unit_renderer(&graphics, &resources);
    LevelRenderer level_renderer(&graphics, &resources, &game.level, &game_view, &unit_renderer);
    AudioRenderer audio_renderer(&audio);
    LevelWindow *level_window = new LevelWindow(graphics.width - sidebar_width, graphics.height - StatusWindow::window_height, &game_view, &level_renderer, &audio_renderer, &resources);
    ChatWindow *chat_window = new ChatWindow(200, graphics.height, &graphics, node_interface);
    ChatUpdater chat_updater(chat_window);
    node_interface->subscribe(&chat_updater);

    WindowPainter window_painter(&resources);
    UiLoop loop(&graphics, 25, &window_painter);

    UnitInfoWindow *unit_info_window = new UnitInfoWindow(unit_info_window_x, unit_info_window_y, UnitInfoWindow::unit_info_window_width, UnitInfoWindow::unit_info_window_height, &resources);
    MapWindow *map_window = new MapWindow(sidebar_position, 0, sidebar_width, map_window_height, &game_view, level_window, &graphics, &resources);
    StackWindow *stack_window = new StackWindow(sidebar_position, 200, sidebar_width, StackWindow::window_height, &resources, &game_view, &unit_renderer, unit_info_window);
    MessageWindow *message_window = new MessageWindow(sidebar_position, map_window_height + stack_window_height, sidebar_width, message_window_height, &graphics, &game_view);
    StatusWindow *status_window = new StatusWindow(0, level_window->height, graphics.width, status_window_height, &graphics, &game_view);

    CombatScreen combat_screen(&resources, &graphics, &audio, &unit_renderer);
    pre_updater.combat_screen = &combat_screen;

    EditorWindow *editor_window = new EditorWindow(&game_view, level_window);
    PaletteWindow *palette_window = new PaletteWindow(&game, &game_view, level_window, editor_window);

    BackgroundWindow *bw = new BackgroundWindow(&loop, &generator, &game, &game_view, node_interface, &level_renderer, palette_window);
    loop.set_root_window(bw);
    bw->add_child(level_window);
    bw->add_child(editor_window);
    bw->add_child(palette_window);
    bw->add_child(map_window);
    bw->add_child(stack_window);
    bw->add_child(message_window);
    bw->add_child(status_window);
    bw->add_child(chat_window);
    bw->add_child(unit_info_window);
    TopWindow *tw = new TopWindow(&graphics, &audio);
    bw->add_child(tw);
    log_statistics("beginning of loop");
    loop.run();
    log_statistics("end of loop");

    // We have to explicitly "stop" the game view, so that any current ghosts can be destroyed.
    // This unlocks any locked stacks, which could release a lot of blocked messages.  If this
    // happened in the game view's destructor, it could result in messages being sent to
    // subscribers that have already been destroyed.  It's all pretty yucky.
    game_view.stop();

    node_interface->stop();
    delete node_interface;

    resources.clear();

    audio.stop();
}

bool parse_options(int argc, char *argv[], Options& options) {

    namespace po = boost::program_options;

    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("server", "run in server mode")
        ("connect", po::value<std::string>()->value_name("ADDRESS"), "connect to server")
        ("load", po::value<std::string>()->value_name("FILENAME"), "load game file")
        ("width", po::value<int>()->value_name("WIDTH"), "screen width")
        ("height", po::value<int>()->value_name("HEIGHT"), "screen height")
        ("fullscreen", "fullscreen mode")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return false;
    }

    if (vm.count("server")) {
        options.server_mode = true;
    } else {
        options.server_mode = false;
    }

    if (vm.count("connect")) {
        options.client_mode = true;
        options.host_addr = vm["connect"].as<std::string>();
    } else {
        options.client_mode = false;
    }

    if (vm.count("load")) {
        options.load_filename = vm["load"].as<std::string>();
    }

    if (vm.count("fullscreen")) {
        options.fullscreen = true;
        options.width = 0;
        options.height = 0;
    } else {
        options.fullscreen = false;
        options.width = 800;
        options.height = 600;
    }

    if (vm.count("width")) {
        options.width = vm["width"].as<int>();
    }

    if (vm.count("height")) {
        options.height = vm["height"].as<int>();
    }

    return true;
}

};


using namespace hex;

int main(int argc, char *argv[]) {
    try {
        Options options;
        if (parse_options(argc, argv, options))
            run(options);
    } catch (Error &ex) {
        BOOST_LOG_TRIVIAL(fatal) << "Failed with: " << ex.what();
    } catch (boost::program_options::error& ex) {
        std::cout << "Error: " << ex.what();
    }

    return 0;
}
