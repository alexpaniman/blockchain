#pragma once

#include "crypto.h"
#include "broadcast.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <fstream>
#include <limits>
#include <optional>
#include <thread>
#include <vector>
#include <array>
#include <unordered_map>
#include <cassert>

#define LOG_ID node_id_
#include "log.h"

// == ACTION

struct action {
    char vote;
};

struct block_data {
    char votes[32 - 8];
    uint8_t count_votes;

    void act(action action) {
        assert(!is_full());
        votes[count_votes ++] = action.vote;
    }

    bool is_full() {
        return count_votes == 3;
    }
};

// =========


constexpr uint32_t BLOCK_MAGIC = 'P'*256*256*256 + 'F'*256*256 + 'N'*256 + 'S';
constexpr uint32_t PROOF_ORDER = 22;


using hash256_t = std::array<uint32_t, 8>;

namespace std {
    template <>
    struct hash<hash256_t> {
        size_t operator()(const hash256_t& key) const noexcept {
            size_t result = 0;
            for (const auto& value : key)
                result ^= std::hash<uint32_t>{}(value);

            return result;
        }
    };
}

template <>
struct std::formatter<hash256_t>: std::formatter<uint32_t> {
    auto format(const hash256_t& hash, std::format_context& ctx) const {
        auto out = ctx.out();
        for (size_t i = 0; i < hash.size(); ++i)
            std::format_to(ctx.out(), "{:08X}", hash[i]);
        return out;
    }
};


struct block {
    uint32_t pow_signature;
    hash256_t previous_hash;

    block_data data;

    hash256_t calculate_hash() const {
        hash256_t hash;
        hash_with_sha_256(this, sizeof(block), hash.data());

        return hash;
    }

    bool verify() const {
        uint32_t mask = (1 << PROOF_ORDER) - 1;
        return (calculate_hash()[0] & mask) == 0;
    }
};

enum class transaction_type: uint16_t {
    DISCOVER      = 0b00,
    SYNC          = 0b01,
    NOTIFY_SIGNED = 0b10,
    ACT           = 0b11
};


inline const char* get_transaction_name(transaction_type type) {
    switch (type) {
    case transaction_type::DISCOVER:      return "DISCOVER";
    case transaction_type::SYNC:          return "SYNC";
    case transaction_type::NOTIFY_SIGNED: return "NOTIFY_SIGNED";
    case transaction_type::ACT:           return "ACT";
    default: assert(false && "Unhandeled transaction type");
    }
}


struct transaction {
    uint32_t magic = BLOCK_MAGIC;
    uint16_t channel;
    transaction_type type;
    uint32_t sequence_number = 0;

    union {
        block signed_block;
        action act;
    };
};


using arranged_block_index = std::size_t;

// Proxy used to quickly access blocks
class arranged_block {
public:
    arranged_block(const block &the_block):
        the_block_(the_block),
        hash_(the_block.calculate_hash()),
        next_() { // List is empty for newly created blocks
    }

    void add_successor(arranged_block_index successor) {
        next_.emplace_back(successor);
    }

    const hash256_t &hash() { return hash_; }
    block &data() { return the_block_; }

    const std::vector<arranged_block_index> &successors() { return next_; }
    std::size_t size() { return next_.size(); }

private:
    block the_block_;
    hash256_t hash_;

    // TODO: this could really be a small vector
    std::vector<arranged_block_index> next_;
};


struct arranged_block_iterable_proxy {
    arranged_block_iterable_proxy(std::vector<arranged_block> &blocks, arranged_block_index current_index):
        blocks_(&blocks),
        current_index_(current_index) {

    }

    class iterator {
    public:
        iterator(std::vector<arranged_block> &blocks, arranged_block_index parent, int successor_index):
            blocks_(blocks),
            parent_(parent),
            successor_index_(successor_index) {}

        iterator operator++() {
            return {blocks_, parent_, successor_index_ + 1};
        }

        arranged_block_iterable_proxy operator*() {
            assert(blocks_[parent_].successors().size() > successor_index_);
            return {blocks_, blocks_[parent_].successors()[successor_index_]};
        }

        auto operator<=>(const iterator &other) const {
            assert(parent_ == other.parent_);
            assert(&blocks_ == &other.blocks_);
            return successor_index_ <=> other.successor_index_;
        }

        bool operator==(const iterator &other) const { return ((*this) <=> other) == 0; };
        bool operator!=(const iterator &other) const { return ((*this) <=> other) != 0; };
        bool operator< (const iterator &other) const { return ((*this) <=> other) <  0; };
        bool operator> (const iterator &other) const { return ((*this) <=> other) >  0; };
        bool operator>=(const iterator &other) const { return ((*this) <=> other) >= 0; };
        bool operator<=(const iterator &other) const { return ((*this) <=> other) <= 0; };

    private:
        std::vector<arranged_block> &blocks_;
        arranged_block_index parent_;
        int successor_index_;
    };

    iterator begin() { return {*blocks_, current_index_, 0}; }
    iterator   end() { return {*blocks_, current_index_, static_cast<int>(unproxy().size() - 1)}; }

    arranged_block &unproxy() { return (*blocks_)[current_index_]; }

    decltype(auto) hash() { return unproxy().hash();  }
    decltype(auto) data() { return unproxy().data(); }
    decltype(auto) size() { return unproxy().size(); }

    arranged_block_index index() { return current_index_; }

private:
    arranged_block_index current_index_;
    std::vector<arranged_block> *blocks_;
};


template <typename network_type>
class blockchain {
public:
    blockchain(int node_id, uint16_t channel, network_type &&net):
        node_id_(node_id),
        net_(std::move(net)),
        channel_(channel),
        arranged_blocks_(),
        block_registry_(),
        pending_blocks_(),
        current_sequence_number_(0) {

        LOG("INIT: signing initial block - in progress");

        arranged_blocks_.push_back(block {});
        sign_block(arranged_blocks_.back().data());
        block_registry_[arranged_blocks_.back().hash()] = arranged_blocks_.size() - 1;

        LOG("INIT: signing initial block - done: {}", arranged_blocks_.back().hash());

        transaction sync {
            .channel = channel,
            .type = transaction_type::DISCOVER,
            .sequence_number = current_sequence_number_ ++,
        };
        broadcast(sync);

        LOG("INIT: broadcasting DISCOVER");
    }

private:
    int node_id_;

    network_type net_;
    uint16_t channel_;

    static constexpr arranged_block_index initial_block_index = 0;

    std::vector<arranged_block> arranged_blocks_;
    std::unordered_map<hash256_t, arranged_block_index> block_registry_;

    std::vector<block> pending_blocks_;

    struct pending_block {
        block the_block;
        bool is_replaced = false; // this is set when another block with the same parent gets signed 
    };
    std::deque<pending_block> pow_blocks_;

    std::optional<block> current_block_;

    uint32_t current_sequence_number_;
    std::unordered_map<address, uint32_t> sequence_numbers_;



    uint32_t random_pow_signature() {
        return ((uint32_t) rand() << 16) | (uint32_t) rand();
    }

    bool sign_block(block &new_block, std::chrono::milliseconds timeout = std::numeric_limits<std::chrono::milliseconds>::max()) {
        auto start = std::chrono::high_resolution_clock::now();
        while (!try_signing_block(new_block)) {
            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = start - now;

            if (elapsed >= timeout)
                return false;
        }

        return true;
    }

    bool try_signing_block(block &new_block) {
        assert(!new_block.verify());

        new_block.pow_signature = random_pow_signature();
        bool is_verified = new_block.verify();

        if (is_verified)
            LOG("SIGNING: successfully signed: {}", new_block.calculate_hash());

        return is_verified;
    } 

    bool is_block_duplicate(const block &block) {
        if (block_registry_.find(block.calculate_hash()) != block_registry_.end())
            return true;

        for (const auto &orphan: pending_blocks_)
            if (orphan.calculate_hash() == block.calculate_hash())
                return true;

        return false;
    }

    bool add_block(const block &new_block) {
        assert(new_block.verify());

        if (is_block_duplicate(new_block)) {
            LOG("RECIEVE: discarding duplicate: {}", new_block.calculate_hash());
            return true; // It's a duplicate
        }

        hash256_t hash = new_block.previous_hash;

        auto parent_iter = block_registry_.find(hash);
        if (parent_iter == block_registry_.end())
            return false; // We don't know anything about block's parent

        auto [_, block_index] = *parent_iter;
        arranged_block &parent = arranged_blocks_[block_index];

        arranged_blocks_.emplace_back(new_block);
        arranged_block_index index = arranged_blocks_.size() - 1;

        parent.add_successor(index);
        LOG("LINK: {} to {}", parent.hash(), parent.successors().size(), arranged_blocks_[index].hash());

        block_registry_[arranged_blocks_.back().hash()] = index;

        // Mark blocks this block replaced
        for (auto &[block, is_replaced]: pow_blocks_) {
            if (block.previous_hash == hash)
                is_replaced = true;
        }

        return true;
    }

    void receive_block(const block &new_block) {
        if (!new_block.verify()) {
            LOG("RECEIVE: discarding (wrong PoW): {}", new_block.calculate_hash());
            return; // discard the block, it's not signed properly
        }

        bool has_parent = add_block(new_block);
        if (!has_parent) {
            pending_blocks_.emplace_back(new_block);
            LOG("RECEIVE: orphan marked pending: {}", new_block.calculate_hash());
        }
    }

    void send_sync(address requester_address) {
        // Send all blocks we have directly to the requester:
        for (arranged_block &block: arranged_blocks_) {
            transaction sync {
                .channel = channel_,
                .type = transaction_type::SYNC,
                .sequence_number = current_sequence_number_ ++,
                .signed_block = block.data()
            };

            // TODO: Why it doesn't work??
            // net_.send(sync, requester_address);

            net_.broadcast(sync);
            LOG("SYNC: sending: {} <- {}", requester_address.to_string(), block.hash());
        }
    }

    struct subtree {
        int depth;
        arranged_block_index leaf;
    };

    subtree find_longest(arranged_block_iterable_proxy block, int depth = 0) {
        if (block.size() == 0)
            return {depth, block.index()};

        arranged_block_index max_element = 0;
        int max_depth = -1;

        for (auto &&next: block) {
            subtree from_next = find_longest(next, depth + 1);
            if (from_next.depth > max_depth) {
                max_depth = from_next.depth;
                max_element = from_next.leaf;
            }
        }

        return {max_depth, max_element};
    }

    arranged_block_iterable_proxy find_longest() {
        return {arranged_blocks_, find_longest(root()).leaf};
    }

    void broadcast_act(action act) {
        LOG("ACT: broadcasting act event '{}'", act.vote);

        transaction act_transaction {
            .channel = channel_,
            .type = transaction_type::ACT,
            .sequence_number = current_sequence_number_ ++,
            .act = act
        };
        broadcast(act_transaction);
    }

    void act(action act) {
        assert(!current_block_ || current_block_->data.is_full());

        if (!current_block_) {
            // blockchain considers longest chain to be the correct one
            auto &&last_block = find_longest();
            current_block_ = block {
                .pow_signature = 0 /* to be calculated */,
                .previous_hash = last_block.hash(),
                .data = {} // TODO: Initial block state?
            };
        }

        current_block_->data.act(act);
        if (current_block_->data.is_full()) {
            pow_blocks_.push_back({ *current_block_ }); // stage this block for signing
            current_block_ = std::nullopt; // current block is done
        }
    }

    void listen() {
        while (true) {
            address sender_address;
            transaction incoming_transaction;
            if (!net_.receive(incoming_transaction, &sender_address)) 
                return;

            if (incoming_transaction.sequence_number < sequence_numbers_[sender_address]) {
                // TODO: Why it always doubles? WHYYYY??
                // LOG("LISTEN: discarded transaction - wrong seqno: {}, expected {}",
                //     incoming_transaction.sequence_number,
                //     sequence_numbers_[sender_address] - 1
                // );
                continue;
            }


            if (incoming_transaction.magic != BLOCK_MAGIC) {
                LOG("LISTEN: discarded transaction - wrong magic: {}", sender_address.to_string());
                continue;
            }

            LOG("LISTEN: received transaction {} (with seqno: {}, was: {}, channel: {}) from {}",
                get_transaction_name(incoming_transaction.type),
                incoming_transaction.sequence_number,
                sequence_numbers_[sender_address],
                incoming_transaction.channel,
                sender_address.to_string());

            if (incoming_transaction.channel != channel_) {
                LOG("LISTEN: discarded transaction - wrong channel {} instead of {}: {}",
                    incoming_transaction.channel, channel_, sender_address.to_string());
                continue;
            }

            sequence_numbers_[sender_address] = incoming_transaction.sequence_number + 1;

            switch (incoming_transaction.type) {
            case transaction_type::ACT:
                act(incoming_transaction.act);
                break;

            case transaction_type::DISCOVER:
                send_sync(sender_address);
                break;

            case transaction_type::NOTIFY_SIGNED:
                receive_block(incoming_transaction.signed_block);
                break;

            case transaction_type::SYNC:
                receive_block(incoming_transaction.signed_block);
                break;
            }
        }
    }

    void notify_signed(block &new_block) {
        assert(new_block.verify());

        transaction signed_new {
            .channel = channel_,
            .type = transaction_type::NOTIFY_SIGNED,
            .sequence_number = current_sequence_number_ ++,
            .signed_block = new_block,
        };

        broadcast(signed_new);
        LOG("NOTIFY: broadcasting newly signed {}", pow_blocks_.front().the_block.calculate_hash());
    }

    void update_pending() {
        bool updated = false;

        do {
            // One of the pending blocks might be a parent of another pending block,
            // we need to repeat the process to process all of them:
            updated = false;

            for (auto it = pending_blocks_.begin(); it != pending_blocks_.end(); ) {
                if (add_block(*it)) {
                    LOG("PENDING: removed processed: {}", it->calculate_hash());
                    it = pending_blocks_.erase(it);

                    updated = true;
                } else {
                    ++ it;
                }
            }
        } while (updated);
    }

    void try_signing(std::chrono::milliseconds timeout) {
        if (pow_blocks_.empty())
            return;

        // TODO: verify parent

        // Remove blocks that got replaced
        while (!pow_blocks_.empty() && pow_blocks_.front().is_replaced) {
            LOG("DISCARDING: unsigned, parent: {}", pow_blocks_.front().the_block.previous_hash);
            pow_blocks_.pop_front();
        }

        if (pow_blocks_.empty())
            return;

        if (sign_block(pow_blocks_.front().the_block, timeout)) {
            notify_signed(pow_blocks_.front().the_block);
            bool has_parent = add_block(pow_blocks_.front().the_block);
            assert(has_parent);

            pow_blocks_.pop_front();
        }
    }

    bool check_need_to_act(const std::string& filename, char& out_char) {
        std::ifstream file(filename);

        if (file.is_open()) {
            file.get(out_char);
            return true;
        }

        return false;
    }

    void act_if_requested() {
        char vote;
        if (check_need_to_act("act", vote)) {
            LOG("ACT: registered need to act with '{}'", vote);

            action action { vote };

            act(action);
            broadcast_act(action);
        }
    }

    void broadcast(transaction message) {
        message.sequence_number = current_sequence_number_ ++;
        net_.broadcast(message);
    }

    char who_wins() {
        auto longest = find_longest();

        char counts[256] = {};
        while (true) {
            if (longest.index() == 0)
                break;

            for (int i = 0; i < longest.data().data.count_votes; ++ i) {
                counts[longest.data().data.votes[i]] ++;
            }

            auto next_index = block_registry_[longest.data().previous_hash];
            longest = arranged_block_iterable_proxy{arranged_blocks_, next_index};
        }

        char winner = '0';
        int max_votes = 0;
        for (int candidate = 0; candidate < 256; ++ candidate) {
            if (max_votes < counts[candidate]) {
                max_votes = counts[candidate];
                winner = candidate;
            }
        }

        return winner;
    }

public:
    void run() {
        static constexpr std::chrono::milliseconds min_iteration_time{1000};

        while (true) {
            LOG("STATUS pow signing: {}, pending: {}, total: {}, current votes: {}",
                pow_blocks_.size(),
                pending_blocks_.size(),
                arranged_blocks_.size(),
                current_block_ ? current_block_->data.count_votes : 0);

            auto start = std::chrono::high_resolution_clock::now();

            listen();
            update_pending();
            try_signing(min_iteration_time);
            act_if_requested();

            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = now - start;

            if (elapsed < min_iteration_time)
                std::this_thread::sleep_for(min_iteration_time - elapsed);
        }
    }

    arranged_block_iterable_proxy root() {
        return {arranged_blocks_, initial_block_index};
    }
};
