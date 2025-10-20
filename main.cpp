#include "server.h"

#include <iostream>

using namespace xiunneg;

int main() {
    auto server_logger =
        std::make_shared<SimpleLogger>("server",
                                       SimpleLoggerInterface::LoggerMode::CONSOLE_ONLY);

    Server::Config config{
        .port = 5050,
        .socket_max_conn = 10240};

    Server server(config);
    server.set_logger(server_logger)
        .run();

    return 0;
}