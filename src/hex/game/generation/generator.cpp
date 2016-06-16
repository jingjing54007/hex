#include "common.h"

#include "hex/basics/hexgrid.h"
#include "hex/basics/noise.h"
#include "hex/game/game.h"
#include "hex/game/game_messages.h"
#include "hex/game/game_updater.h"
#include "hex/game/generation/generator.h"
#include "hex/game/movement/movement.h"
#include "hex/messaging/loader.h"
#include "hex/messaging/updater.h"

static std::string type(Tile& tile) {
    std::vector<std::string> parts;
    boost::split(parts, tile.type->name, boost::is_any_of("_"));
    return parts[0];
}

static std::string subtype(Tile& tile) {
    std::vector<std::string> parts;
    boost::split(parts, tile.type->name, boost::is_any_of("_"));
    if (parts.size() > 1)
        return parts[1];
    else
        return "";
}

void Generator::generate_level() {
    PerlinNoise noise(3, 3);
    PerlinNoise noise2(6, 6);

    PerlinNoise type_noise1(3, 3);
    PerlinNoise type_noise2(3, 3);

    Level& level = game->level;
    std::map<std::string, TileType::pointer>& types = game->tile_types;

    for (int i = 0; i < level.height; i++) {
        for (int j = 0; j < level.width; j++) {
            Tile& tile = level.tiles[i][j];
            if (i == 0 || i == level.height - 1 || j == 0 || j == level.width - 1) {
                tile.type = types["outside"];
                continue;
            }

            float px = (float) j / level.width;
            float py = (float) i / level.height;
            if (j % 2 == 1)
                py += 0.5f / level.height;

            float height = (noise.value(px, py) + noise2.value(py, px)) * height_scale;
            if (height > 0.0)
                height = powf(height, height_power);

            int hill_rand = rand();
            int road_rand = rand();

            if (height < sea_level)
                tile.type = types["water"];
            else {
                float angle = atan2(type_noise1.value(px, py), type_noise2.value(px, py));
                int sector = (int) ((angle + M_PI) * 5 / (2 * M_PI));
                std::string type_name;
                switch (sector) {
                    case 0: type_name = "grass"; break;
                    case 1: type_name = "desert"; break;
                    case 2: type_name = "steppe"; break;
                    case 3: type_name = "wasteland"; break;
                    default: type_name = "snow"; break;
                }

                if (height > mountain_level) {
                    type_name += "_mountain1";
                } else if (height > hill_level && hill_rand % 2) {
                    type_name += "_hill1";
                }

                tile.type = types[type_name];
                if (!tile.type) {
                    throw Error() << boost::format("Unknown tile type: %s") % type_name;
                }

                if (tile.type->has_property(Roadable) && road_rand % 2)
                    tile.road = true;
            }
        }
    }

    // Coalesce hills and mountains
    for (int i = 0; i < level.height; i++) {
        for (int j = 0; j < level.width; j++) {
            Point tile_pos(j, i);
            Tile& tile = level.tiles[tile_pos];
            std::string tile_type = type(tile);
            std::string tile_subtype = subtype(tile);

            int mountain_rand = rand();
            int hill_rand = rand();

            // Hills can be coalesced in two patterns:
            //       H   and   H
            //     h             h
            int left_rand = rand();
            if (tile_subtype == "hill1") {
                bool left = left_rand % 2 == 0;
                int dir = left ? 4 : 2;
                Point neighbour_pos;
                get_neighbour(tile_pos, dir, &neighbour_pos);
                if (!level.contains(neighbour_pos))
                    continue;
                Tile& neighbour = level.tiles[neighbour_pos];
                if (subtype(neighbour) == "hill1") {
                    tile.type = types[tile_type + (left ? "_hill2" : "_hill3")];
                    neighbour.type = types[tile_type + "_hill0"];
                    tile_subtype = subtype(tile);
                }
            }

            // Mountains can be coalesced as follows:
            //       M
            //     m   m
            //       m
            if (tile_subtype == "mountain1") {
                Point neighbour_pos[6];
                get_neighbours(tile_pos, neighbour_pos);
                if (!level.contains(neighbour_pos[2]) || !level.contains(neighbour_pos[3]) || !level.contains(neighbour_pos[4]))
                    continue;
                Tile& neighbour2 = level.tiles[neighbour_pos[2]];
                Tile& neighbour3 = level.tiles[neighbour_pos[3]];
                Tile& neighbour4 = level.tiles[neighbour_pos[4]];
                if (subtype(neighbour2) != "mountain1" || subtype(neighbour3) != "mountain1" || subtype(neighbour4) != "mountain1")
                    continue;

                tile.type = types[tile_type + "_mountain2"];
                neighbour2.type = types[tile_type + "_mountain0"];
                neighbour3.type = types[tile_type + "_mountain0"];
                neighbour4.type = types[tile_type + "_mountain0"];
                tile_subtype = subtype(tile);
            }

            // Mountains can further be coalesced:
            //          M
            //        m   m
            //      m   m   m
            //        m   m
            //          m
            if (tile_subtype == "mountain2") {
                Point neighbour_pos[6];
                get_neighbours(tile_pos, neighbour_pos);
                Point neighbour_pos_l[6];
                get_neighbours(neighbour_pos[4], neighbour_pos_l);
                Point neighbour_pos_r[6];
                get_neighbours(neighbour_pos[2], neighbour_pos_r);
                Point neighbour_pos_b[6];
                get_neighbours(neighbour_pos[3], neighbour_pos_b);
                if (!level.contains(neighbour_pos_b[3]) || !level.contains(neighbour_pos_l[4]) || !level.contains(neighbour_pos_r[2]))
                    continue;
                Tile& neighbourl4 = level.tiles[neighbour_pos_l[4]];
                Tile& neighbourb4 = level.tiles[neighbour_pos_b[4]];
                Tile& neighbourb3 = level.tiles[neighbour_pos_b[3]];
                Tile& neighbourb2 = level.tiles[neighbour_pos_b[2]];
                Tile& neighbourr2 = level.tiles[neighbour_pos_r[2]];
                if (subtype(neighbourl4) != "mountain1" || subtype(neighbourb4) != "mountain1" || subtype(neighbourb3) != "mountain1"
                        || subtype(neighbourb2) != "mountain1" || subtype(neighbourr2) != "mountain1")
                    continue;

                tile.type = types[tile_type + "_mountain3"];
                neighbourl4.type = types[tile_type + "_mountain0"];
                neighbourb4.type = types[tile_type + "_mountain0"];
                neighbourb3.type = types[tile_type + "_mountain0"];
                neighbourb2.type = types[tile_type + "_mountain0"];
                neighbourr2.type = types[tile_type + "_mountain0"];
            }

            if (tile_subtype == "mountain1" && mountain_rand % mountain_culling) {
                tile.type = types[tile_type];
            }

            if (tile_subtype == "hill1" && hill_rand % hill_culling) {
                tile.type = types[tile_type];
            }
        }
    }
}

void Generator::create_game(Updater& updater) {
    updater.receive(create_message(ClearGame));

    srand(seed);

    game = new Game();

    GameUpdater game_updater(game);
    updater.subscribe(&game_updater);

    int width = 57;
    int height = 48;

    ReceiverMessageLoader loader(updater);
    loader.load("data/game.txt");

    game->level.width = width;
    game->level.height = height;
    game->level.tiles.resize(width, height);
    generate_level();

    updater.receive(boost::make_shared<WrapperMessage2<int, int> >(SetLevel, game->level.width, game->level.height));
    for (int i = 0; i < game->level.height; i++) {
        Point origin(0, i);
        std::vector<std::string> data;
        for (int j = 0; j < game->level.width; j++) {
            Tile& tile = game->level.tiles[i][j];
            std::ostringstream data_ss;
            TileType::pointer tile_type = tile.type;
            data_ss << tile_type->name;
            if (tile.road)
                data_ss << "/r";
            data.push_back(data_ss.str());
        }
        updater.receive(boost::make_shared<WrapperMessage2<Point, std::vector<std::string> > >(SetLevelData, origin, data));
    }

    updater.receive(create_message(CreateFaction, 1, "independent", "Independent"));
    updater.receive(create_message(CreateFaction, 2, "orcs", "Orc Hegemony"));
    updater.receive(create_message(CreateFaction, 3, "drow", "Great Drow Empire"));

    for (int i = 1; i <= 40; i++) {
        StrMap<UnitType>::iterator item = game->unit_types.begin();
        std::advance(item, rand() % game->unit_types.size());

        IntMap<Faction>::iterator faction_iter = game->factions.begin();
        std::advance(faction_iter, rand() % game->factions.size());
        int faction = faction_iter->second->id;

        MovementModel movement_model(&game->level);
        Point p(-1, -1);
        for (int j = 0; j < 10; j++) {
            int tx = rand() % game->level.width;
            int ty = rand() % game->level.height;
            if (movement_model.admits(*item->second, *game->level.tiles[ty][tx].type)) {
                p = Point(tx, ty);
                break;
            }
        }

        if (p.x != -1) {
            updater.receive(create_message(CreateStack, i, p, faction));
            int num = (rand() % 5) + 1;
            for (int j = 0; j < num; j++) {
                updater.receive(create_message(CreateUnit, i, item->second->name));
            }
        }
    };

    // Add towers
    for (int i = 0; i < game->level.height; i++) {
        for (int j = 0; j < game->level.width; j++) {
            Point tile_pos(j, i);
            Tile& tile = game->level.tiles[tile_pos];
            if (!tile.type->has_property(Roadable))
                continue;
            Point neighbour_pos[6];
            get_neighbours(tile_pos, neighbour_pos);
            int water_count = 0;
            for (int i = 0; i < 6; i++) {
                if (!game->level.contains(neighbour_pos[i]))
                    continue;
                Tile& neighbour = game->level.tiles[neighbour_pos[i]];
                if (neighbour.type == game->tile_types.get("water"))
                    water_count++;
            }
            if (water_count >= 3 && rand() % 4 == 0) {
                IntMap<Faction>::iterator faction_iter = game->factions.begin();
                std::advance(faction_iter, rand() % game->factions.size());
                int faction = faction_iter->second->id;

                updater.receive(create_message(CreateStructure, tile_pos, "tower", faction));
            } else if (rand() % 100 == 0) {
                updater.receive(create_message(CreateStructure, tile_pos, "wizard_tower", 0));
            }
        }
    }

    game->turn_number = 1;
    updater.receive(create_message(TurnBegin, game->turn_number));

    updater.unsubscribe(&game_updater);
    delete game;
}