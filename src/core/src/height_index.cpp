// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/height_index.hpp"

namespace hitokoto::core {

void HeightIndex::assign(const std::vector<double>& /*heights*/) {}

double HeightIndex::height(std::size_t /*index*/) const {
    return 0;
}

void HeightIndex::set(std::size_t /*index*/, double /*height*/) {}

void HeightIndex::insert(std::size_t /*index*/, std::size_t /*count*/, double /*height*/) {}

void HeightIndex::erase(std::size_t /*index*/, std::size_t /*count*/) {}

double HeightIndex::sumBefore(std::size_t /*index*/) const {
    return 0;
}

std::size_t HeightIndex::indexAt(double /*y*/) const {
    return 0;
}

std::size_t HeightIndex::slotOf(std::size_t index) const noexcept {
    return index;
}

void HeightIndex::rebuild(std::size_t /*gapAt*/, std::size_t /*gapLength*/) {}

void HeightIndex::moveGap(std::size_t /*to*/) {}

void HeightIndex::add(std::size_t /*slot*/, double /*delta*/) {}

}  // namespace hitokoto::core
