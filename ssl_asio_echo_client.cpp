#include <iostream>
#include <string>
#include <array>
#include <asio.hpp>
#include <asio/ssl.hpp>

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

asio::ssl::context create_ssl_context() {
    asio::ssl::context ctx{ asio::ssl::context::tls_client };

    ctx.set_options(
        asio::ssl::context::default_workarounds |
        asio::ssl::context::no_sslv2 |
        asio::ssl::context::no_sslv3 |
        asio::ssl::context::no_tlsv1 |
        asio::ssl::context::no_tlsv1_1 |
        asio::ssl::context::single_dh_use
    );

    ctx.load_verify_file("server.crt");
    ctx.set_verify_mode(asio::ssl::verify_peer);

    return ctx;
}

void echo(const std::string& ip, int port, const std::string& message) {
    asio::io_context ioc{};

    auto sslCtx = create_ssl_context();
    asio::ssl::stream<asio::ip::tcp::socket> ssl_connection{ ioc, sslCtx };

    auto& raw_socket = ssl_connection.next_layer();
    asio::ip::tcp::endpoint ep{ asio::ip::make_address(ip), (asio::ip::port_type)port };
    raw_socket.connect(ep);

    ssl_connection.handshake(asio::ssl::stream_base::client);
    asio::write(ssl_connection, asio::buffer(message));

    std::array<char, 256> buf{};
    ssl_connection.read_some(asio::buffer(buf));
    std::cout << "returned: " << buf.data() << "\n";

    ssl_connection.shutdown();
    raw_socket.shutdown(asio::ip::tcp::socket::shutdown_both);
    raw_socket.close();
}

// g++ asio_echo_client.cpp -I asio/include -I libressl/include -L libressl/tls -L libressl/ssl -L libressl/crypto -l ws2_32 -l tls -l ssl -l crypto -o client
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