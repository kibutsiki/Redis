#include <cassert>
#include "hashtable.h"

static void h_init(Htab* htab, std::size_t n) {
    assert(n > 0 && ((n - 1) & n) == 0);
    htab->tab.assign(n, nullptr);
    htab->mask = n - 1;
    htab->size = 0;
}

static void h_insert(Htab* htab, HNode* node) {
    assert(htab != nullptr);
    assert(node != nullptr);
    assert(!htab->tab.empty());

    const std::size_t pos = node->hcode & htab->mask;
    HNode* next = htab->tab[pos];

    node->next = next;
    htab->tab[pos] = node;
    htab->size++;
}

static HNode** h_lookup(
    Htab* htab,
    HNode* key,
    bool (*eq)(HNode*, HNode*)
) {
    if (htab == nullptr ||
        key == nullptr ||
        eq == nullptr ||
        htab->tab.empty()) {
        return nullptr;
    }

    const std::size_t pos = key->hcode & htab->mask;
    HNode** from = &htab->tab[pos];

    for (HNode* cur; (cur = *from) != nullptr; from = &cur->next) {
        if (cur->hcode == key->hcode && eq(cur, key)) {
            return from;
        }
    }

    return nullptr;
}

HNode* hm_lookup(
    HMap* hmap,
    HNode* key,
    bool (*eq)(HNode*, HNode*)
) {
    if (hmap == nullptr) {
        return nullptr;
    }

    HNode** from = h_lookup(&hmap->newer, key, eq);
    if (from == nullptr) {
        from = h_lookup(&hmap->older, key, eq);
    }

    return from == nullptr ? nullptr : *from;
}

void hm_insert(HMap* hmap, HNode* node) {
    assert(hmap != nullptr);
    assert(node != nullptr);

    if (hmap->newer.tab.empty()) {
        h_init(&hmap->newer, 4);
    }

    h_insert(&hmap->newer, node);
}

HNode* hm_delete(
    HMap* hmap,
    HNode* key,
    bool (*eq)(HNode*, HNode*)
) {
    if (hmap == nullptr) {
        return nullptr;
    }

    HNode** from = h_lookup(&hmap->newer, key, eq);
    Htab* owner = &hmap->newer;
    if (from == nullptr) {
        from = h_lookup(&hmap->older, key, eq);
        owner = &hmap->older;
    }
    if (from == nullptr) {
        return nullptr;
    }

    HNode* node = *from;
    *from = node->next;
    node->next = nullptr;
    --owner->size;
    return node;
}

void hm_clear(HMap* hmap) {
    if (hmap == nullptr) {
        return;
    }

    for (Htab* table : {&hmap->newer, &hmap->older}) {
        for (HNode*& bucket : table->tab) {
            for (HNode* node = bucket; node != nullptr;) {
                HNode* next = node->next;
                node->next = nullptr;
                node = next;
            }
            bucket = nullptr;
        }
        table->size = 0;
    }
}

std::size_t hm_size(HMap* hmap) {
    if (hmap == nullptr) {
        return 0;
    }

    return hmap->newer.size + hmap->older.size;
}

void hm_foreach(HMap* hmap, HForeach callback, void* arg) {
    if (hmap == nullptr || callback == nullptr) {
        return;
    }

    for (Htab* table : {&hmap->newer, &hmap->older}) {
        for (HNode* bucket : table->tab) {
            for (HNode* node = bucket; node != nullptr; node = node->next) {
                if (!callback(node, arg)) {
                    return;
                }
            }
        }
    }
}