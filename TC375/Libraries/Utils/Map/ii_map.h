#ifndef LIBRARIES_UTILS_MAP_II_MAP_H_
#define LIBRARIES_UTILS_MAP_II_MAP_H_

#include <stdbool.h>
#include <stdint.h>

#define IIMAP_MAX_ARR_SIZE 16

typedef struct
{
    uint32_t key;
    uint32_t value;
} IINode;

typedef struct
{
    IINode nodeArr[IIMAP_MAX_ARR_SIZE];
    int nodeCnt;
} IIMap;

void iiMap_init (IIMap *m);
bool iiMap_insert (IIMap *m, uint32_t key, uint32_t value);
bool iiMap_find (const IIMap *m, uint32_t key, uint32_t *out);
bool iiMap_update (IIMap *m, uint32_t key, uint32_t value);
bool iiMap_delete (IIMap *m, uint32_t key);
bool iiMap_upsert_or_replace (IIMap *m, uint32_t key, uint32_t value);

#endif /* LIBRARIES_UTILS_MAP_II_MAP_H_ */
