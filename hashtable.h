#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

struct HNode {
    HNode* next{};
    uint64_t hcode{};
};

struct Htab {
    std::vector<HNode*> tab;
    std::size_t mask{};
    std::size_t size{};
};

struct HMap {
    Htab newer{};
    Htab older{};
    std::size_t migrate_pos{};
};

using HForeach = bool (*)(HNode* node, void* arg);

HNode* hm_lookup(HMap* hmap, HNode* key, bool (*eq)(HNode*, HNode*));
void hm_insert(HMap* hmap, HNode* node);
HNode* hm_delete(HMap* hmap, HNode* key, bool (*eq)(HNode*, HNode*));
void hm_clear(HMap* hmap);
std::size_t hm_size(HMap* hmap);
void hm_foreach(HMap* hmap, HForeach callback, void* arg);