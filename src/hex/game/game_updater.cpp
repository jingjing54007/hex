#include "common.h"

#include "hex/basics/error.h"
#include "hex/messaging/serialiser.h"
#include "hex/messaging/message.h"
#include "hex/messaging/receiver.h"
#include "hex/game/game.h"
#include "hex/game/game_serialisation.h"
#include "hex/game/game_messages.h"
#include "hex/game/game_updater.h"


GameUpdater::GameUpdater(Game *game): game(game) {
}

GameUpdater::~GameUpdater() {
}

void GameUpdater::receive(boost::shared_ptr<Message> update) {
    game->message_id = update->id;
    apply_update(update);
}

void GameUpdater::apply_update(boost::shared_ptr<Message> update) {
    switch (update->type) {
        case SetLevel: {
            boost::shared_ptr<WrapperMessage2<int, int> > upd = boost::dynamic_pointer_cast<WrapperMessage2<int, int> >(update);
            game->level.resize(upd->data1, upd->data2);
        } break;

        case SetLevelData: {
            boost::shared_ptr<WrapperMessage2<Point, std::vector<std::string> > > upd = boost::dynamic_pointer_cast<WrapperMessage2<Point, std::vector<std::string> > >(update);
            Point offset = upd->data1;
            std::vector<std::string>& tile_data = upd->data2;
            for (unsigned int i = 0; i < tile_data.size(); i++) {
                Point tile_pos(offset.x + i, offset.y);
                if (!game->level.contains(tile_pos)) {
                    std::cerr << "Tile coordinate " << tile_pos << " is outside the level" << std::endl;
                    continue;
                }
                std::string& tile_type_name = tile_data[i];
                TileType *tile_type = game->tile_types[tile_type_name];
                game->level.tiles[tile_pos].type = tile_type;
            }
        } break;

        case CreateTileType: {
            boost::shared_ptr<WrapperMessage<TileType> > upd = boost::dynamic_pointer_cast<WrapperMessage<TileType> >(update);
            game->create_tile_type(upd->data);
        } break;

        case CreateUnitType: {
            boost::shared_ptr<WrapperMessage<UnitType> > upd = boost::dynamic_pointer_cast<WrapperMessage<UnitType> >(update);
            game->create_unit_type(upd->data);
        } break;

        case CreateFaction: {
            boost::shared_ptr<CreateFactionMessage> upd = boost::dynamic_pointer_cast<CreateFactionMessage>(update);
            game->create_faction(upd->data1, upd->data2, upd->data3);
        } break;

        case CreateStack: {
            boost::shared_ptr<CreateStackMessage> upd = boost::dynamic_pointer_cast<CreateStackMessage>(update);
            game->create_unit_stack(upd->data1, upd->data2, upd->data3);
        } break;

        case CreateUnit: {
            boost::shared_ptr<CreateUnitMessage> upd = boost::dynamic_pointer_cast<CreateUnitMessage>(update);
            game->create_unit(upd->data1, upd->data2);
        } break;

        case FactionReady: {
            boost::shared_ptr<FactionReadyMessage> upd = boost::dynamic_pointer_cast<FactionReadyMessage>(update);
            Faction *faction = game->get_faction(upd->data1);
            if (faction != NULL) {
                faction->ready = upd->data2;
            }
        } break;

        case TurnEnd: {
            boost::shared_ptr<TurnEndMessage> upd = boost::dynamic_pointer_cast<TurnEndMessage>(update);
            game->turn_number = upd->data;
        } break;

        case UnitMove: {
            boost::shared_ptr<UnitMoveMessage> upd = boost::dynamic_pointer_cast<UnitMoveMessage>(update);

            // check unit can move
            UnitStack *stack = game->stacks[upd->data1];
            if (stack == NULL) {
                warn("No stack with id %d", upd->data1);
                return;
            }

            Point& new_pos = upd->data2.back();
            UnitStack *target_stack = game->level.tiles[new_pos].stack;

            // TODO but not if some units were left behind
            game->level.tiles[stack->position].stack = NULL;

            if (target_stack != NULL) {
                trace("absorb!");
                target_stack->absorb(stack);
            }

            if (stack->units.size() == 0) {
                game->destroy_unit_stack(stack->id);
            } else {
                stack->position = new_pos;
                game->level.tiles[stack->position].stack = stack;
            }
        } break;

        default:
            break;
    }
}
