#include "common.h"

#include "hexgame/game/game.h"
#include "hexgame/game/combat/combat.h"
#include "hexgame/game/combat/combat_model.h"
#include "hexgame/game/combat/move_types.h"


namespace hex {

CombatModel CombatModel::default_combat_model;

CombatModel::~CombatModel() {
    for (auto iter = move_types.begin(); iter != move_types.end(); iter++) {
        delete iter->second;
    }
}

std::vector<const MoveType *> CombatModel::get_available_move_types(const Battle& battle, const Participant& participant) const {
    std::vector<const MoveType *> types;

    for (auto iter = move_types.begin(); iter != move_types.end(); iter++) {
        Atom type = iter->first;
        const MoveType *move_type = iter->second;
        if (participant.unit->get_property<int>(type) > 0) {
            types.push_back(move_type);
        }
    }
    return types;
}

const MoveType *CombatModel::get_move_type(const Move& move) const {
    auto iter = move_types.find(move.type);
    if (iter == move_types.end()) {
        throw Error() << "Cannot find move type for " << move.type;
    }

    return iter->second;
}

void CombatModel::populate_move_types() {
    move_types[Archery] = new ArcheryMoveType();
    move_types[Charge] = new ChargeMoveType();
    move_types[Healing] = new HealingMoveType();
    move_types[Strike] = new StrikeMoveType();
    move_types[Riposte] = new RiposteMoveType();
}

};
