#ifndef AI_H
#define AI_H

#include "hexgame/game/game.h"


class AiUpdater;
class MessageReceiver;
class MessageQueue;

class Ai {
public:
    Ai(const std::string& faction_type, MessageReceiver *dispatcher);
    ~Ai();

    void start();
    void stop();
    MessageReceiver *get_receiver() const;

private:
    void run_thread();
    void update();
    void update_unit_stack(UnitStack& stack);
    UnitStack::pointer get_nearest_enemy(UnitStack& stack);

private:
    Game game;
    std::string faction_type;
    AiUpdater *updater;
    MessageQueue *receiver;
    MessageReceiver *dispatcher;
    unsigned int update_step;
    Faction::pointer faction;
    boost::atomic<bool> started;
    boost::thread ai_thread;

    friend class AiUpdater;
};

#endif