#pragma once

#include <iostream>

namespace hex::mcp {

    class Client {
    public:
        Client() = default;
        ~Client() = default;

        int run(std::istream &input, std::ostream &output);
    };

}
