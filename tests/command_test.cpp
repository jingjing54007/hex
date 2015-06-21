#include "common.h"

#include "hex/messaging/serialiser.h"
#include "hex/messaging/message.h"
#include "hex/messaging/receiver.h"
#include "hex/messaging/updater.h"
#include "hex/game/game.h"
#include "hex/game/game_messages.h"
#include "hex/game/game_serialisation.h"
#include "hex/game/game_updater.h"

#define ORIGIN 1


class RecordUpdateListener: public MessageReceiver {
public:
    RecordUpdateListener(Serialiser &writer): writer(writer) { }
    virtual ~RecordUpdateListener() { }

    virtual void receive(boost::shared_ptr<Message> update) {
        writer << update.get();
        std::cerr << "Recorded: " << update << std::endl;
    }

private:
    Serialiser &writer;
};


class DebugUpdateListener: public MessageReceiver {
public:
    DebugUpdateListener() { }
    virtual ~DebugUpdateListener() { }

    virtual void receive(boost::shared_ptr<Message> update) {
        std::ostringstream buf;
        Serialiser writer(buf);
        writer << update.get();
        std::cout << "Applied: " << buf.str();
    }
};


int main(int argc, char *argv[]) {

    Game game;
    Updater updater(ORIGIN);
    GameUpdater game_updater(&game);
    updater.subscribe(&game_updater);

    if (argc <= 1) {
        return 1;
    }

    if (!strcmp(argv[1], "record")) {
        Serialiser writer(std::cout);
        RecordUpdateListener listener(writer);
        updater.subscribe(&listener);

        std::vector<Point> path;
        path.push_back(Point(3,7));
        path.push_back(Point(4,7));
        path.push_back(Point(4,8));

        updater.receive(boost::make_shared<PlayerNameMessage>(0, "Alice"));

        updater.receive(boost::make_shared<UnitMoveMessage>(0, path));

        updater.receive(boost::make_shared<PlayerReadyMessage>(0, true));

        updater.receive(boost::make_shared<PlayerReadyMessage>(1, true));

        updater.receive(boost::make_shared<Message>(StreamClose));
    } else if (!strcmp(argv[1], "replay")) {
        DebugUpdateListener listener;
        updater.subscribe(&listener);

        Deserialiser reader(std::cin);

        while (std::cin.good()) {
            Message *update_ptr;
            reader >> update_ptr;
            if (update_ptr == NULL)
                break;
            boost::shared_ptr<Message> update(update_ptr);
            updater.receive(update);
            if (update->type == StreamClose)
                break;
        }
    } else {
        return 1;
    }
}
