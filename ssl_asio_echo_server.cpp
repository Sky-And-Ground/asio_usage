#include <iostream>
#include <utility>
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

template<std::size_t N>
void ssl_send_all(asio::ssl::stream<asio::ip::tcp::socket>& connection, const std::array<char, N>& buf, std::size_t len) {
    asio::error_code ec;
    std::size_t bytes_written = 0;

    while (bytes_written < len) {
        std::size_t written = connection.write_some(asio::buffer(buf.data() + bytes_written, len - bytes_written), ec);

        if (ec) {
            std::cerr << "one ssl connection send failed, " << ec.value() << ", " << ec.message() << "\n";
            return;
        }

        bytes_written += written;
    }
}

void ssl_handle_connection(asio::ssl::stream<asio::ip::tcp::socket>&& ssl_connection) {
    asio::error_code ec;

    auto& raw_socket = ssl_connection.next_layer();
    auto ep = raw_socket.remote_endpoint(ec);
    if (ec) {
        std::cerr << "get remote endpoint failed, " << ec.value() << ", " << ec.message() << "\n";
        return;
    }

    const std::string ip = ep.address().to_string();
    const auto port = ep.port();

    ssl_connection.handshake(asio::ssl::stream_base::server, ec);
    if (ec) {
        std::cerr << "ssl hand shake failed, " << ec.value() << ", " << ec.message() << "\n";
        return;
    }

    std::array<char, 256> buf;
    buf.fill('\0');

    std::size_t len = ssl_connection.read_some(asio::buffer(buf), ec);

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
    ssl_send_all(ssl_connection, buf, len);

    ssl_connection.shutdown();
    raw_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    raw_socket.close();
}

asio::ssl::context create_ssl_context() {
    asio::ssl::context ctx{ asio::ssl::context::tls_server };

    ctx.set_options(
        asio::ssl::context::default_workarounds |
        asio::ssl::context::no_sslv2 |
        asio::ssl::context::no_sslv3 |
        asio::ssl::context::no_tlsv1 |
        asio::ssl::context::no_tlsv1_1 |
        asio::ssl::context::single_dh_use
    );

    ctx.use_certificate_chain_file("server.crt");
    ctx.use_private_key_file("server.key", asio::ssl::context::pem);

    return ctx;
}

void start_echo_server(int port) {
    asio::error_code ec;
    asio::io_context ioc;
    asio::ip::tcp::endpoint ep{ asio::ip::tcp::v4(), (asio::ip::port_type)port };
    asio::ip::tcp::acceptor acc{ ioc };

    asio::ssl::context sslCtx = create_ssl_context();

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
            asio::ssl::stream<asio::ip::tcp::socket> connection{ std::move(client), sslCtx };
            ssl_handle_connection(std::move(connection));
        }
    }
}

/*
    This program's ssl just uses the libressl library, version 4.2.1
    compiled with visual studio 17 2022, and generate the tls.lib, ssl.lib, crypto.lib and a openssl.exe

    the server.crt and server.key could be generated with the openssl.exe and the following commands, 
    which is called as self signed certificate:

    openssl genrsa -out server.key 2048
    openssl req -new -key server.key -out server.csr -subj "/C=CN/ST=Beijing/L=Beijing/O=MyCompany/CN=localhost"
    openssl x509 -req -days 365 -in server.csr -signkey server.key -out server.crt

    then server just use both, client just uses the server.crt.
*/
// windows:
// g++ asio_echo_server.cpp -I asio/include -I libressl/include -L libressl/tls -L libressl/ssl -L libressl/crypto -l ws2_32 -l tls -l ssl -l crypto -o server
//
// linux:
// g++ asio_echo_server.cpp -DASIO_STANDALONE -I /root/asio_usage/asio-master/asio/include -I /home/gzy_test/libressl-4.2.1/build/include 
// -L /home/gzy_test/libressl-4.2.1/build/tls -L /home/gzy_test/libressl-4.2.1/build/ssl -L /home/gzy_test/libressl-4.2.1/build/crypto 
// -l ssl -l crypto -l tls -lpthread -std=c++11
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
