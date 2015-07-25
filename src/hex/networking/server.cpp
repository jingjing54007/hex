#include "common.h"

#include "hex/basics/error.h"
#include "hex/messaging/message.h"
#include "hex/game/game.h"
#include "hex/game/game_serialisation.h"
#include "hex/game/game_messages.h"
#include "hex/game/game_updater.h"
#include "hex/networking/networking.h"

Server::Server(int port, MessageReceiver *receiver):
        port(port), receiver(receiver), io_service(), acceptor(io_service), shutdown_requested(false),
        next_connection_id(1), next_message_id(1), max_backlog_size(1000) {

}

Server::~Server() {

}

void Server::start() {
    server_thread = boost::thread(&Server::run_thread, this);
}

void Server::stop() {
    shutdown_requested = true;
    io_service.stop();
    server_thread.join();
}

void Server::receive(boost::shared_ptr<Message> msg) {
    if (msg->origin == 0)
        io_service.post(boost::bind(&Server::broadcast, this, msg));
    else
        io_service.post(boost::bind(&Server::receive_from_network, this, msg));
}

void Server::broadcast(boost::shared_ptr<Message> msg) {
    msg->id = next_message_id++;
    message_backlog[msg->id] = msg;
    for (std::map<int, Connection::pointer>::iterator iter = connections.begin(); iter != connections.end(); iter++) {
        iter->second->send_message(msg);
    }
    while (message_backlog.size() > max_backlog_size)
        message_backlog.erase(message_backlog.begin());
}

void Server::receive_from_network(boost::shared_ptr<Message> msg) {
    Connection::pointer source_connection = connections[msg->origin];
    if (msg->type == StreamReplay) {
        boost::shared_ptr<WrapperMessage<int> > replay = boost::dynamic_pointer_cast<WrapperMessage<int> >(msg);
        int replay_from = replay->data;

        for (std::map<int, boost::shared_ptr<Message> >::iterator iter = message_backlog.begin(); iter != message_backlog.end(); iter++) {
            if (iter->first > replay_from) {
                source_connection->send_message(iter->second);
            }
        }
    }

    trace("Received a message");

    receiver->receive(msg);
}

void Server::run_thread() {
    tcp::endpoint endpoint(tcp::v4(), port);
    acceptor.open(endpoint.protocol());
    acceptor.set_option(boost::asio::socket_base::reuse_address(true));
    acceptor.bind(endpoint);
    acceptor.listen(5);
    start_accept();
    io_service.run();
}

void Server::start_accept() {
    Connection::pointer new_connection = Connection::create(acceptor.get_io_service(), this);
    acceptor.async_accept(new_connection->socket, boost::bind(&Server::handle_accept, this, new_connection, boost::asio::placeholders::error));
}

void Server::handle_accept(Connection::pointer new_connection, const boost::system::error_code& error) {
    if (!error) {
        new_connection->start();
        new_connection->send_message(boost::make_shared<WrapperMessage<std::string> >(StreamOpen, std::string("Hex Server 0.1")));
        new_connection->id = next_connection_id++;
        connections[new_connection->id] = new_connection;
    }
    if (!shutdown_requested)
        start_accept();
}