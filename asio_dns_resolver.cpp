#include <iostream>
#include <asio.hpp>

// g++ asio_dns_resolver.cpp -DASIO_STANDALONE -I asio/include -l ws2_32
// g++ asio_dns_resolver.cpp -DASIO_STANDALONE -I asio/include -std=c++11 -lpthread
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <hostname>\n";
        return 1;
    }

    asio::error_code ec;
    asio::io_context ioc;
    asio::ip::tcp::resolver resolver{ ioc };

    auto results = resolver.resolve(argv[1], "", ec);
    if (ec) {
        std::cerr << "resolve failed, " << ec.value() << ", " << ec.message() << "\n";
        return 1;
    }

    for (const auto& item : results) {
        auto endpoint = item.endpoint();
        std::cout << endpoint.address().to_string() << "\n";
    }

    return 0;
}
