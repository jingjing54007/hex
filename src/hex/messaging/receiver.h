#ifndef RECEIVER_H
#define RECEIVER_H

class Message;

class MessageReceiver {
public:
    virtual ~MessageReceiver() { }
    virtual void receive(boost::shared_ptr<Message> msg) = 0;
};

extern void replay_messages(const std::string& filename, MessageReceiver& receiver);
extern void replay_messages(std::istream& input, MessageReceiver& receiver, const std::string& input_name);

#endif
