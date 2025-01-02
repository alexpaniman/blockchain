#include "broadcast.h"
#include "blockchain.h"

constexpr int PORT = 12345;

int main() {
    network net(PORT);
    blockchain chain(0, 0, std::move(net));
    chain.run();
}
