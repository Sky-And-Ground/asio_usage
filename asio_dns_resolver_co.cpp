#include <iostream>
#include <vector>
#include <string>
#include <asio.hpp>

asio::awaitable<std::vector<std::string>> resolve(asio::io_context& ioc, const std::string& hostname) {
    std::vector<std::string> vec;

    try {
        asio::ip::tcp::resolver resolver{ ioc };

        auto results = co_await resolver.async_resolve(hostname, "", asio::use_awaitable);
        for (const auto& item : results) {
            auto endpoint = item.endpoint();
            vec.emplace_back(endpoint.address().to_string());
        }
    }
    catch(const std::exception& e) {
        std::cerr << "resolve error: " << e.what() << "\n";
    }

    co_return vec;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <hostname>\n";
        return 1;
    }

    const char* hostname = argv[1];
    asio::io_context ioc;

    asio::co_spawn(ioc, [&ioc, hostname]() -> asio::awaitable<void> {
        auto results = co_await resolve(ioc, hostname);

        for (const auto& addr : results) {
            std::cout << addr << "\n";
        }
    }, asio::detached);

    ioc.run();
    return 0;
}
