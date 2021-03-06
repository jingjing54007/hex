#include "common.h"

#include "hexutil/messaging/message.h"

#include "hexgame/game/game.h"
#include "hexgame/game/game_messages.h"
#include "hexgame/game/combat/combat.h"

#include "hexview/view/pre_updater.h"
#include "hexview/view/view.h"
#include "hexview/view/combat/combat_screen.h"


namespace hex {

PreUpdater::PreUpdater(Game *game):
        game(game), combat_screen(NULL) {
}

PreUpdater::~PreUpdater() {
}

void PreUpdater::receive(Message *update) {
    try {
        apply_update(update);
    } catch (const DataError& err) {
        BOOST_LOG_TRIVIAL(error) << "Invalid update received; " << err.what();
    }
}

void PreUpdater::apply_update(Message *update) {
    switch (update->type) {
        case DoBattle: {
            auto upd = dynamic_cast<DoBattleMessage *>(update);
            int attacker_id = upd->data1;
            Point attacking_point = game->stacks.get(attacker_id)->position;
            Point attacked_point = upd->data2;
            std::vector<Move>& moves = upd->data3;
            Battle battle(game, attacked_point, attacking_point, moves);
            if (combat_screen)
                combat_screen->show_battle(&battle);
        } break;

        default:
            break;
    }
}

};
