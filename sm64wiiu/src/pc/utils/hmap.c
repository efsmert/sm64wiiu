#include <stddef.h>
#include <stdlib.h>

#include "data/dynos_cmap.cpp.h"

struct HMapEntry {
    int64_t key;
    void* value;
};

struct HMap {
    struct HMapEntry* entries;
    size_t len;
    size_t cap;
    size_t iter;
};

static struct HMap* hmap_cast(void* map) {
    return (struct HMap*)map;
}

static ptrdiff_t hmap_find(const struct HMap* map, int64_t key) {
    size_t i = 0;
    for (i = 0; i < map->len; i++) {
        if (map->entries[i].key == key) {
            return (ptrdiff_t)i;
        }
    }
    return -1;
}

void* hmap_create(bool useUnordered) {
    struct HMap* map = (struct HMap*)calloc(1, sizeof(struct HMap));
    (void)useUnordered;
    return map;
}

void* hmap_get(void* map, int64_t key) {
    struct HMap* hmap = hmap_cast(map);
    ptrdiff_t index = 0;
    if (hmap == NULL) {
        return NULL;
    }
    index = hmap_find(hmap, key);
    return (index >= 0) ? hmap->entries[index].value : NULL;
}

void hmap_put(void* map, int64_t key, void* value) {
    struct HMap* hmap = hmap_cast(map);
    ptrdiff_t index = 0;
    if (hmap == NULL) {
        return;
    }

    index = hmap_find(hmap, key);
    if (index >= 0) {
        hmap->entries[index].value = value;
        return;
    }

    if (hmap->len == hmap->cap) {
        size_t newCap = (hmap->cap == 0) ? 32 : (hmap->cap * 2);
        struct HMapEntry* newEntries = (struct HMapEntry*)realloc(hmap->entries, newCap * sizeof(struct HMapEntry));
        if (newEntries == NULL) {
            return;
        }
        hmap->entries = newEntries;
        hmap->cap = newCap;
    }

    hmap->entries[hmap->len].key = key;
    hmap->entries[hmap->len].value = value;
    hmap->len++;
}

void hmap_del(void* map, int64_t key) {
    struct HMap* hmap = hmap_cast(map);
    ptrdiff_t index = 0;
    if (hmap == NULL) {
        return;
    }
    index = hmap_find(hmap, key);
    if (index < 0) {
        return;
    }

    hmap->entries[index] = hmap->entries[hmap->len - 1];
    hmap->len--;
}

void hmap_clear(void* map) {
    struct HMap* hmap = hmap_cast(map);
    if (hmap == NULL) {
        return;
    }
    hmap->len = 0;
    hmap->iter = 0;
}

void hmap_destroy(void* map) {
    struct HMap* hmap = hmap_cast(map);
    if (hmap == NULL) {
        return;
    }
    free(hmap->entries);
    free(hmap);
}

size_t hmap_len(void* map) {
    struct HMap* hmap = hmap_cast(map);
    if (hmap == NULL) {
        return 0;
    }
    return hmap->len;
}

void* hmap_begin(void* map) {
    struct HMap* hmap = hmap_cast(map);
    if (hmap == NULL || hmap->len == 0) {
        return NULL;
    }
    hmap->iter = 0;
    return hmap->entries[0].value;
}

void* hmap_next(void* map) {
    struct HMap* hmap = hmap_cast(map);
    if (hmap == NULL || hmap->len == 0) {
        return NULL;
    }

    hmap->iter++;
    if (hmap->iter >= hmap->len) {
        return NULL;
    }
    return hmap->entries[hmap->iter].value;
}
