/*
 * Created by root on 2026/7/27.
 */

#include <linux/ktime.h>

#include "uptime.h"
#include "../include/ksm_internal.h"

u64 ksm_uptime(void)
{
    return ktime_get_seconds() - ksm_start_time;
}
