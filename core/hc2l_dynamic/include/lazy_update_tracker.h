// lazy_update_tracker.h
#pragma once

#include <unordered_set>
#include <utility>
#include <cstdint>
#include <cstddef>
#include <functional>

namespace road_network {

using NodeID = uint32_t;
using DirtyPair = std::pair<NodeID, NodeID>;

// Hash function for unordered_set of pairs
struct DirtyPairHash {
    size_t operator()(const DirtyPair& p) const {
        return std::hash<NodeID>()(p.first) ^ (std::hash<NodeID>()(p.second) << 1);
    }
};

// Global dirty pair tracker
extern std::unordered_set<DirtyPair, DirtyPairHash> dirty_pairs;

// Adds a (u, v) query to the dirty set
void mark_dirty(NodeID u, NodeID v);

// Checks if a query (u, v) is in the dirty set
bool is_dirty(NodeID u, NodeID v);

// Clear all dirty entries (optional utility)
void clear_dirty();

} // namespace road_network
