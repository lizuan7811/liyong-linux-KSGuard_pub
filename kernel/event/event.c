/*
 * Created by root on 2026/7/27.
 */

#include <linux/atomic.h>

#include "event.h"

/* 這裡是 event_count 唯一的定義處，其他檔案一律用 extern (ksm_internal.h) */
atomic64_t event_count;

void ksm_event_init(void)
{
    atomic64_set(&event_count, 0);
}

/* 修正: 原本是空函式，沒有真的做遞增，/proc/ksm_status 的 Events 永遠是 0 */
void ksm_event_inc(void)
{
    atomic64_inc(&event_count);
}

/* 修正: 原本沒有 return，函式回傳值是未定義行為 */
u64 ksm_event_count(void)
{
    return atomic64_read(&event_count);
}
