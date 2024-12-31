#pragma once

#include "buffer.h"
#include "broadcast.h"

#include <map>
#include <vector>
#include <cstdint>
#include <mutex>


struct packet {
    address sender;
    std::vector<char> data;
};

class simulation {
public:
    simulation(std::map<address, std::vector<packet>> &senders,
               uint32_t address,
               std::mutex *mutex):
        address_(address),
        senders_(&senders),
        mutex_(mutex) {
    }

    bool send(buffer message, address target);
    bool broadcast(buffer message);
    bool receive(buffer out_message, address *out_sender_addr);

private:
    uint32_t address_;
    std::map<address, std::vector<packet>> *senders_;
    std::mutex *mutex_;
};

class simulation_builder {
public:
    simulation_builder(): address_(0) {}
    simulation produce_node() { return {senders_, address_ ++, &mutex_}; }

private:
    uint32_t address_;
    std::map<address, std::vector<packet>> senders_;
    std::mutex mutex_;
};

