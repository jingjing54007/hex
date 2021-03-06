#ifndef COMBAT_MODEL
#define COMBAT_MODEL

#include "hexutil/basics/properties.h"


namespace hex {

class Battle;
class Participant;
class MoveType;

class CombatModel {
public:
    static CombatModel *get_default() {
        return &default_combat_model;
    }

    CombatModel() {
        populate_move_types();
    }
    ~CombatModel();

    std::vector<const MoveType *> get_available_move_types(const Battle& battle, const Participant& participant) const;

    const MoveType *get_move_type(const Move& move) const;

private:
    void populate_move_types();

public:
    std::unordered_map<Atom, MoveType *> move_types;

private:
    static CombatModel default_combat_model;
};

};

#endif
