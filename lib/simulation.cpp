#include "simulation.h"

#include <arpa/inet.h>
#include <climits>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <ifaddrs.h>
#include <mutex>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>


struct address_impl { uint8_t data[4]; };

address to_address(uint16_t id) {
    address addr{};
    memcpy(&addr, &id, sizeof(id));

    return addr;
}


bool simulation::send(buffer message, address target_addr) {
    std::lock_guard<std::mutex> lock(*mutex_);

    std::vector<char> data{
        static_cast<char*>(message.data),
        static_cast<char*>(message.data) + message.size
    };

    packet sent_packet {to_address(address_), std::move(data)};
    (*senders_)[target_addr].push_back(std::move(sent_packet));

    return true;
}

bool simulation::broadcast(buffer message) {
    std::lock_guard<std::mutex> lock(*mutex_);

    std::vector<char> data{
        static_cast<char*>(message.data),
        static_cast<char*>(message.data) + message.size
    };

    packet sent_packet {to_address(address_), std::move(data)};
    for (auto &[sender_address, packets]: *senders_)
        packets.push_back(std::move(sent_packet));

    return true;
}

bool simulation::receive(buffer out_message, address *out_sender_addr) {
    std::lock_guard<std::mutex> lock(*mutex_);

    auto &packets = (*senders_)[to_address(address_)];
    if (packets.empty())
        return false;

    packet received = std::move(packets.back());
    packets.pop_back();

    // TODO: receive part's of the packet...

    memcpy(out_message.data, &*received.data.begin(), out_message.size);
    *out_sender_addr = received.sender;

    return true;
}

