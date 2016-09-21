#ifndef PUBLISHER_H
#define PUBLISHER_H

#include "hex/messaging/receiver.h"


class Publisher: public MessageReceiver {
public:
    Publisher();
    Publisher(int id);
    virtual ~Publisher();

    void subscribe(MessageReceiver *receiver);
    void unsubscribe(MessageReceiver *receiver);
    virtual void receive(boost::shared_ptr<Message> msg);

private:
    void send_update_to_subscribers(boost::shared_ptr<Message> update);

private:
    int id;
    int next_message_id;

    std::vector<MessageReceiver *> subscribers;
};


#endif