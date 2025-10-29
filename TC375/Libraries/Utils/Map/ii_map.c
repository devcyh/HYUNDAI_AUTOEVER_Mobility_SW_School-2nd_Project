#include "ii_map.h"
#include "stm.h"

void iiMap_init (IIMap *m)
{
    m->nodeCnt = 0;
}

bool iiMap_insert (IIMap *m, uint32_t key, uint32_t value)
{
    if (!m)
        return false;

    if (m->nodeCnt >= IIMAP_MAX_ARR_SIZE)
        return false;

    for (int i = 0; i < m->nodeCnt; i++)
    {
        if (m->nodeArr[i].key == key)
            return false;
    }

    m->nodeArr[m->nodeCnt].key = key;
    m->nodeArr[m->nodeCnt].value = value;
    m->nodeCnt++;
    return true;
}

bool iiMap_find (const IIMap *m, uint32_t key, uint32_t *out)
{
    if (!m)
        return false;

    for (int i = 0; i < m->nodeCnt; i++)
    {
        if (m->nodeArr[i].key == key)
        {
            if (out)
                *out = m->nodeArr[i].value;
            return true;
        }
    }
    return false;
}

bool iiMap_update (IIMap *m, uint32_t key, uint32_t value)
{
    if (!m)
        return false;

    for (int i = 0; i < m->nodeCnt; i++)
    {
        if (m->nodeArr[i].key == key)
        {
            m->nodeArr[i].value = value;
            return true;
        }
    }
    return false;
}

bool iiMap_delete (IIMap *m, uint32_t key)
{
    if (!m)
        return false;

    for (int i = 0; i < m->nodeCnt; i++)
    {
        if (m->nodeArr[i].key == key)
        {
            int lastIdx = --m->nodeCnt;
            m->nodeArr[i] = m->nodeArr[lastIdx];
            return true;
        }
    }
    return false;
}

bool iiMap_upsert_or_replace (IIMap *m, uint32_t key, uint32_t value)
{
    if (!m)
        return false;

    // 1. 이미 존재하는 key면 value 업데이트
    for (int i = 0; i < m->nodeCnt; i++)
    {
        if (m->nodeArr[i].key == key)
        {
            m->nodeArr[i].value = value;
            return true;
        }
    }

    // 2. 공간이 남아 있으면 추가
    if (m->nodeCnt < IIMAP_MAX_ARR_SIZE)
    {
        m->nodeArr[m->nodeCnt].key = key;
        m->nodeArr[m->nodeCnt].value = value;
        m->nodeCnt++;
        return true;
    }

    // 3. 공간이 없으면 랜덤으로 하나 덮어쓰기
    int idx = STM0_getTime10ns() % IIMAP_MAX_ARR_SIZE;
    m->nodeArr[idx].key = key;
    m->nodeArr[idx].value = value;

    return true;
}
