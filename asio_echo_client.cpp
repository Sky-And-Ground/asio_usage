#include <iostream>
#include <string>
#include <array>
#include <asio.hpp>

int parse_port(const char* param) noexcept {
    int result = 0;

    while (*param) {
        if (result > 65535) {
            return -1;
        }

        if (isdigit(*param)) {
            result = 10 * result + (*param - '0');
        }
        else {
            return -1;
        }

        ++param;
    }

    if (result > 65535) {
        return -1;
    }

    return result;
}

void echo(const std::string& ip, int port, const std::string& message) {
    asio::io_context ioc{};
    asio::ip::tcp::socket s{ ioc };
    asio::ip::tcp::endpoint ep{ asio::ip::make_address(ip), (asio::ip::port_type)port };

    s.connect(ep);
    asio::write(s, asio::buffer(message));

    std::array<char, 256> buf{};
    s.read_some(asio::buffer(buf));
    std::cout << "returned: " << buf.data() << "\n";
}

// g++ asio_echo_client.cpp -I asio/include -l ws2_32 -o client
int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "echo client usage: " << argv[0] << " <ip> <port>\n";
        return 1;
    }

    int port = parse_port(argv[2]);
    if (port < 0) {
        std::cerr << "invalid port\n";
        return 1;
    }

    std::string message;
    std::cout << "your message: ";
    std::getline(std::cin, message);
    
    echo(argv[1], port, message);
    return 0;
}