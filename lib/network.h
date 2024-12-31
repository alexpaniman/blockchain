#pragma once

#include "broadcast.h"

template <typename type>
concept distributed_network = requires(type net, buffer message, address target, address* out_sender_addr, buffer out_message) {
    { net.send(message, target) } -> std::convertible_to<bool>;
    { net.broadcast(message) } -> std::convertible_to<bool>;
    { net.receive(out_message, out_sender_addr) } -> std::convertible_to<bool>;
};

