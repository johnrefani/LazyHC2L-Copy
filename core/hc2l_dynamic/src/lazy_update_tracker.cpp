#include "lazy_update_tracker.h"
#include <algorithm> // for std::minmax

namespace road_network {

std::unordered_set<DirtyPair, DirtyPairHash> dirty_pairs;

void mark_dirty(NodeID u, NodeID v) {
    dirty_pairs.insert(std::minmax(u, v));
}

bool is_dirty(NodeID u, NodeID v) {
    return dirty_pairs.count(std::minmax(u, v)) > 0;
}

void clear_dirty() {
    dirty_pairs.clear();
}

} // namespace road_network
