/*
*	@author yuan
*	@brief  ping program written in c++11 with non-boost asio.
*/
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include <iostream>
#include <array>
#include <thread>
#include <chrono>
#include <cstdint>
#include <asio.hpp>

using namespace std::chrono;

// rfc 791.
struct alignas(4) IpHeader {
	uint8_t version_and_ihl;
	uint8_t service_type;
	uint16_t total_length;
	uint16_t identification;
	uint16_t flags_and_fragment_offset;
	uint8_t time_to_live;
	uint8_t protocol;
	uint16_t header_checksum;
	uint32_t source_address;
	uint32_t destination_address;

	// cause the options and padding have no usage in this program, they are just ignored.
};

// rfc 792.
struct alignas(2) IcmpHeader {
	uint8_t type;
	uint8_t code;
	uint16_t checksum;
	uint16_t identifier;
	uint16_t sequence;
};

uint16_t calc_checksum(const uint16_t* data, size_t len) {
	uint32_t result = 0;

	while (len > 1) {
		result += *data;
		++data;
		len -= 2;
	}

	if (len > 0) {
		result += *reinterpret_cast<const uint8_t*>(data);
	}

	result = (result >> 16) + (result & 0xffff);
	result += (result >> 16);
	return static_cast<uint16_t>(~result);
}

uint16_t get_current_process_id() {
#ifdef _WIN32
	return (uint16_t)GetCurrentProcessId();
#else
	return getpid();
#endif
}

void ping(asio::io_context& ioc, const asio::ip::icmp::endpoint& ep, int try_times, int timeout_millisec, int packet_size) {
	asio::error_code ec;
	asio::ip::icmp::socket sock{ ioc };

	sock.open(asio::ip::icmp::v4(), ec);
	if (ec) {
		std::cerr << "open socket with icmp failed, " << ec.value() << ", " << ec.message() << "\n";
		return;
	}

	const int SEND_DURATION_SEC = 1;

	int send_count = 0;
	int recv_count = 0;
	int lost_count = 0;

	uint16_t pid = get_current_process_id();

	std::array<char, 1024> buf{};

	for (int i = 0; i < try_times; ++i) {
		// build packet.
		IcmpHeader* icmp_header_part = (IcmpHeader*)buf.data();
		icmp_header_part->type = 8;
		icmp_header_part->code = 0;
		icmp_header_part->identifier = ::htons(pid);
		icmp_header_part->sequence = ::htons(i);

		char* data_part = buf.data() + sizeof(IcmpHeader);
		for (int j = 0; j < packet_size; ++j) {
			data_part[j] = 'A' + (j % 26);   // just let it in A - Z.
		}

		icmp_header_part->checksum = 0;
		icmp_header_part->checksum = calc_checksum((const uint16_t*)buf.data(), sizeof(IcmpHeader) + packet_size * sizeof(char));

		// tick.
		auto time_start = steady_clock::now();

		sock.send_to(asio::buffer(buf.data(), sizeof(IcmpHeader) + packet_size * sizeof(char)), ep, 0, ec);
		if (ec) {
			std::cerr << "send failed, " << ec.value() << ", " << ec.message() << "\n";
			++lost_count;
			std::this_thread::sleep_for(seconds(SEND_DURATION_SEC));
			continue;
		}

		++send_count;

		buf.fill('\0');
		asio::ip::icmp::endpoint sender_ep;
		size_t totalRecvLen = sock.receive_from(asio::buffer(buf), sender_ep, 0, ec);

		auto time_end = steady_clock::now();
		
		if (ec) {
			if (ec.value() == asio::error::timed_out) {
				std::cerr << "timed out\n";
			}

			++lost_count;
		}
		else {
			IpHeader* reply_ip_header = (IpHeader*)buf.data();
			uint32_t ip_header_length = 4 * (reply_ip_header->version_and_ihl & 0x0f);

			IcmpHeader* reply_icmp_header = (IcmpHeader*)(buf.data() + ip_header_length);

			if (reply_icmp_header->type == 0 && 
				reply_icmp_header->code == 0 &&
				::ntohs(reply_icmp_header->identifier) == pid) {
				++recv_count;

				std::cout << "Reply from " << sender_ep.address().to_string() << ": ";
				std::cout << "bytes=" << (totalRecvLen - ip_header_length) << " ";
				std::cout << "time=" << duration_cast<milliseconds>(time_end - time_start).count() << "ms ";
				std::cout << "TTL=" << (int32_t)(reply_ip_header->time_to_live) << " ";
				std::cout << "seq=" << ::ntohs(reply_icmp_header->sequence);
				std::cout << "\n";
			}
			else {
				std::cerr << "unexpected reply\n";
				++lost_count;
			}
		}

		std::this_thread::sleep_for(seconds(SEND_DURATION_SEC));
	}

	std::cout << "\n";
	std::cout << "packets: sent: " << send_count << ", ";
	std::cout << "recv: " << recv_count << ", ";
	std::cout << "lost: " << lost_count << "\n";
}

int main(int argc, char* argv[]) {
	if (argc != 2) {
		std::cerr << "ping usage: " << argv[0] << " host\n";
		return 0;
	}

	asio::error_code ec;
	asio::io_context ioc;
	asio::ip::icmp::resolver host_resolver{ ioc };

	auto results = host_resolver.resolve(argv[1], "", ec);
	if (ec) {
		std::cerr << "resolve host failed, " << ec.value() << ", " << ec.message() << "\n";
		return 1;
	}

	// just try one.
	const int packet_size = 32;
	const int try_times = 4;
	const int timeout_millisec = 1000;

	for (const auto& item : results) {
		const auto& ep = item.endpoint();

		std::cout << "Ping " << argv[1] << " [" << ep.address().to_string() << "] with " << packet_size << " bytes of data:\n\n";
		ping(ioc, ep, try_times, timeout_millisec, packet_size);
		break;
	}

	return 0;
}