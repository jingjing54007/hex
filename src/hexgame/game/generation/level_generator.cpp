#include "common.h"

#ifdef _WIN32
#define _USE_MATH_DEFINES
#include <math.h>
#endif

#include "hexutil/basics/hexgrid.h"
#include "hexutil/basics/noise.h"

#include "hexgame/game/game.h"
#include "hexgame/game/game_messages.h"
#include "hexgame/game/generation/generator.h"
#include "hexgame/game/generation/level_generator.h"
#include "hexgame/game/generation/utils.h"
#include "hexgame/game/movement/movement.h"
#include "hexgame/game/movement/pathfinding.h"


namespace hex {

Atom feature(Tile& tile) {
    return tile.feature_type->name;
}

void LevelGenerator::generate_level(int width, int height) {
    BOOST_LOG_TRIVIAL(info) << boost::format("Generating level of size (%d, %d)") % width % height;
    level.width = width;
    level.height = height;
    level.tiles.resize(width, height);

    generate_terrain();
    coalesce_mountains();
    add_rivers();
    add_forests();
    add_bridges();
    add_roads();
}

void LevelGenerator::generate_terrain() {
    PerlinNoise noise(3, 3);
    PerlinNoise noise2(6, 6);

    PerlinNoise type_noise1(3, 3);
    PerlinNoise type_noise2(3, 3);

    for (int i = 0; i < level.height; i++) {
        for (int j = 0; j < level.width; j++) {
            Tile& tile = level.tiles[i][j];
            if (i == 0 || i == level.height - 1 || j == 0 || j == level.width - 1) {
                tile.type = types["outside"];
                tile.feature_type = feature_types["outside"];
                continue;
            }

            float px = (float) j / level.width;
            float py = (float) i / level.height;
            if (j % 2 == 1)
                py += 0.5f / level.height;

            float height = (noise.value(px, py) + noise2.value(py, px)) * generator->height_scale;
            if (height > 0.0)
                height = powf(height, generator->height_power);

            int hill_rand = rand();

            float angle = atan2(type_noise1.value(px, py), type_noise2.value(px, py));
            int sector = (int) ((angle + M_PI) * 5 / (2 * M_PI));
            if (height < generator->sea_level) {
                tile.type = (sector == 4) ? types["ice"] : types["water"];
                tile.feature_type = (sector == 4) ? feature_types["ice"] : feature_types["water"];
            } else {
                Atom type_name;
                switch (sector) {
                    case 0: type_name = "grass"; break;
                    case 1: type_name = "desert"; break;
                    case 2: type_name = "steppe"; break;
                    case 3: type_name = "wasteland"; break;
                    default: type_name = "snow"; break;
                }

                tile.type = types[type_name];
                if (!tile.type) {
                    throw Error() << boost::format("Unknown tile type: %s") % type_name;
                }

                Atom feature_type_name = "plains";

                if (height > generator->mountain_level) {
                    feature_type_name = "mountain1";
                } else if (height > generator->hill_level && hill_rand % 2) {
                    feature_type_name = "hill1";
                }

                tile.feature_type = feature_types[feature_type_name];
                if (!tile.feature_type) {
                    throw Error() << boost::format("Unknown feature type: %s") % feature_type_name;
                }
            }
        }
    }
}

void LevelGenerator::coalesce_mountains() {
    BOOST_LOG_TRIVIAL(info) << "Coalescing hills and mountains";
    for (int i = 0; i < level.height; i++) {
        for (int j = 0; j < level.width; j++) {
            Point tile_pos(j, i);
            Tile& tile = level.tiles[tile_pos];

            Atom tile_feature = feature(tile);

            int mountain_rand = rand();
            int hill_rand = rand();

            // Hills can be coalesced in two patterns:
            //       H   and   H
            //     h             h
            int left_rand = rand();
            if (tile_feature == "hill1") {
                bool left = left_rand % 2 == 0;
                int dir = left ? 4 : 2;
                Point neighbour_pos = get_neighbour(tile_pos, dir);
                if (!level.contains(neighbour_pos))
                    continue;
                Tile& neighbour = level.tiles[neighbour_pos];
                if (feature(neighbour) == "hill1") {
                    tile.feature_type = feature_types[left ? "hill2" : "hill3"];
                    neighbour.feature_type = feature_types["hill0"];
                    tile_feature = feature(tile);
                }
            }

            // Mountains can be coalesced:
            //          M
            //        m   m
            //      m   m   m
            //        m   m
            //          m
            if (tile_feature == "mountain1") {
                std::vector<Point> covered_points = get_shape_points(tile_pos, feature_types["mountain3"]->get_property<Atom>(Shape));
                if (!level_contains_points(level, covered_points))
                    continue;

                if (!level_feature_types_match(level, covered_points, "mountain1"))
                    continue;

                set_level_feature_type(level, covered_points, feature_types["mountain0"]);
                tile.feature_type = feature_types["mountain3"];
                tile_feature = feature(tile);
            }

            // Mountains can also be coalesced:
            //       M
            //     m   m
            //       m
            if (tile_feature == "mountain1") {
                std::vector<Point> covered_points = get_shape_points(tile_pos, feature_types["mountain2"]->get_property<Atom>(Shape));
                if (!level_contains_points(level, covered_points))
                    continue;

                if (!level_feature_types_match(level, covered_points, "mountain1"))
                    continue;

                set_level_feature_type(level, covered_points, feature_types["mountain0"]);
                tile.feature_type = feature_types["mountain2"];
                tile_feature = feature(tile);
            }

            if (tile_feature == "mountain1" && mountain_rand % generator->mountain_culling) {
                tile.feature_type = feature_types["plains"];
            }

            if (tile_feature == "hill1" && hill_rand % generator->hill_culling) {
                tile.feature_type = feature_types["plains"];
            }
        }
    }
}

void LevelGenerator::add_rivers() {
    BOOST_LOG_TRIVIAL(info) << "Adding rivers";
    TileType::pointer water_type = types["water"];

    for (int i = 0; i < 50; i++) {
        Point start_pos;
        int attempts = 0;
        do {
            start_pos = Point(rand() % level.width, rand() % level.height);
        } while (attempts++ < 20 && level.tiles[start_pos].type != water_type);
        if (attempts >= 20)
            continue;
        for (int j = 0; j < 30; j++) {
            Point next_pos;
            int attempts = 0;
            do {
                next_pos = get_neighbour(start_pos, rand() % 6);
            } while (attempts++ < 20 && (!level.contains(next_pos) || !level.tiles[next_pos].has_property(Roadable)));
            if (attempts >= 20)
                continue;
            Tile& tile = level.tiles[next_pos];
            tile.type = water_type;
            tile.feature_type = feature_types["water"];
            start_pos = next_pos;
        }
    }
}

void LevelGenerator::add_forests() {
    PerlinNoise forest_noise(6, 6);

    BOOST_LOG_TRIVIAL(info) << "Adding forests";
    for (int i = 0; i < level.height; i++) {
        for (int j = 0; j < level.width; j++) {
            float px = (float) j / level.width;
            float py = (float) i / level.height;
            if (j % 2 == 1)
                py += 0.5f / level.height;

            float forest_value = forest_noise.value(px, py);

            Point tile_pos(j, i);
            Tile& tile = level.tiles[tile_pos];

            int forest_rand = rand();
            if (forest_value > 0.0 && feature(tile) == "plains" && forest_rand % 4 != 0) {
                Atom forest_type = (forest_rand % 3 == 0) ? "dead_forest" : "forest";
                tile.feature_type = feature_types[forest_type];
            }
        }
    }
}

void LevelGenerator::add_bridges() {
    BOOST_LOG_TRIVIAL(info) << "Adding bridges";
    TileType::pointer water_type = types["water"];
    FeatureType::pointer bridge_types[] = { feature_types["bridge0"], feature_types["bridge1"], feature_types["bridge2"] };

    for (int i = 0; i < level.width * level.height; i++) {
        Point tile_pos(rand() % level.width, rand() % level.height);
        Tile& tile = level.tiles[tile_pos];
        if (tile.type != water_type)
            continue;
        int water_neighbours = 0;
        int best_dir = -1;
        for (int dir = 0; dir < 3; dir++) {
            Point n1 = get_neighbour(tile_pos, dir);
            Point n2 = get_neighbour(tile_pos, dir + 3);
            if (!level.contains(n1) || !level.contains(n2))
                continue;

            if (level.tiles[n1].type == water_type)
                water_neighbours++;
            if (level.tiles[n2].type == water_type)
                water_neighbours++;

            if (level.tiles[n1].has_property(Roadable) && level.tiles[n2].has_property(Roadable))
                best_dir = dir;
        }
        if (water_neighbours >= 3 && best_dir >= 0) {
            tile.feature_type = bridge_types[best_dir];
        }
    }
}

void LevelGenerator::add_roads() {
    BOOST_LOG_TRIVIAL(info) << "Adding roads";
    for (int i = 0; i < 100; i++) {
        Point start_pos(rand() % level.width, rand() % level.height);
        Point end_pos(start_pos.x + rand() % 10 - rand() % 10, start_pos.y + rand() % 10 - rand() % 10);
        if (!level.contains(end_pos))
            continue;
        MovementModel movement(game);
        Pathfinder pathfinder(&level, &movement);
        UnitStack::pointer party = boost::make_shared<UnitStack>(start_pos, Faction::pointer());
        Unit::pointer unit = boost::make_shared<Unit>();
        unit->type = boost::make_shared<UnitType>();
        unit->properties.set<int>(Walking, 1);
        party->units.push_back(unit);
        pathfinder.start(*party, start_pos, end_pos);
        pathfinder.complete();
        Path path;
        pathfinder.build_path(path);
        path.resize(30);
        for (auto iter = path.begin(); iter != path.end(); iter++) {
            Tile& tile = level.tiles[*iter];
            if (tile.has_property(Roadable) && rand() % 6 != 0) {
                PointNeighbours neighbours = get_neighbours(*iter);
                bool too_many_roads = false;
                for (int i = 0; i < 6; i++) {
                    if (level.tiles[neighbours[i]].has_property(Road) && level.tiles[neighbours[(i+1) % 6]].has_property(Road))
                        too_many_roads = true;
                }
                if (!too_many_roads) {
                    tile.feature_type = feature_types["road"];
                }
            }
        }
    }
}

};
