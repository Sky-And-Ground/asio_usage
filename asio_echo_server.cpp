#include <iostream>
#include <utility>
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

template<std::size_t N>
void send_all(asio::ip::tcp::socket& connection, const std::array<char, N>& buf, std::size_t len) {
    asio::error_code ec;
    std::size_t bytes_written = 0;

    while (bytes_written < len) {
        std::size_t written = connection.write_some(asio::buffer(buf.data() + bytes_written, len - bytes_written), ec);

        if (ec) {
            std::cerr << "one connection send failed, " << ec.value() << ", " << ec.message() << "\n";
            return;
        }

        bytes_written += written;
    }
}

void handle_connection(asio::ip::tcp::socket&& connection) {
    asio::error_code ec;

    auto ep = connection.remote_endpoint(ec);
    if (ec) {
        std::cerr << "get remote endpoint failed, " << ec.value() << ", " << ec.message() << "\n";
        return;
    }

    const std::string ip = ep.address().to_string();
    const auto port = ep.port();

    std::array<char, 256> buf;
    buf.fill('\0');

    std::size_t len = connection.read_some(asio::buffer(buf), ec);
    
    if (ec) {
        if (ec == asio::error::eof) {
            std::cerr << ip << ":" << port << " connection has been closed\n";
        }
        else {
            std::cerr << ip << ":" << port << " read failed, " << ec.value() << ", " << ec.message() << "\n";
        }

        return;
    }

    if (len == 0) {
        return;
    }
    
    std::cout << ip << ":" << port << " " << len << ", " << buf.data() << "\n";
    send_all(connection, buf, len);
    
    connection.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    connection.close();
}

void start_echo_server(int port) {
    asio::error_code ec;
    asio::io_context ioc;
    asio::ip::tcp::endpoint ep{ asio::ip::tcp::v4(), (asio::ip::port_type)port };
    asio::ip::tcp::acceptor acc{ ioc };

    acc.open(ep.protocol(), ec);
    if (ec) {
        std::cerr << "acceptor open failed, " << ec.value() << ", " << ec.message() << "\n";
        return;
    }

    acc.set_option(asio::ip::tcp::acceptor::reuse_address(true), ec);
    if (ec) {
        std::cerr << "set option failed on reuse address, " << ec.value() << ", " << ec.message() << "\n";
        return;
    }

    acc.bind(ep, ec);
    if (ec) {
        std::cerr << "acceptor bind failed, " << ec.value() << ", " << ec.message() << "\n";
        return;
    }

    acc.listen(32, ec);
    if (ec) {
        std::cerr << "acceptor listen failed, " << ec.value() << ", " << ec.message() << "\n";
        return;
    }

    while (true) {
        asio::ip::tcp::socket client{ ioc };
        acc.accept(client, ec);

        if (ec) {
            std::cerr << "acceptor accept failed, " << ec.value() << ", " << ec.message() << "\n";
            return;
        }
        else {
            handle_connection(std::move(client));
        }
    }
}

// g++ asio_echo_server.cpp -I asio/include -l ws2_32 -o server
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "echo server usage: " << argv[0] << " <port>\n";
        return 1;
    }

    int port = parse_port(argv[1]);
    if (port < 0) {
        std::cerr << "invalid port\n";
        return 1;
    }

    start_echo_server(port);
    return 0;
}
