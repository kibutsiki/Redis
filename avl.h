#pragma once

#include <set>
#include <string>
#include <unordered_map>
#include <utility>

struct SortedSet {
    std::set<std::pair<double, std::string>> by_score;
    std::unordered_map<std::string, double> by_name;
};
