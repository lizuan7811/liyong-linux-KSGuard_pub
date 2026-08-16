/*
 * Created by root on 2026/7/27.
 */
#include <linux/seq_file.h>

#include "status.h"

#include "../include/type.h"
#include "../event/event.h"
#include "../time/uptime.h"

extern struct ksm_context kc;

/*
 * 修正: 原本簽名是 (struct seq_file *m, void *v, struct ksm_context *kc)
 * 且是 static，但 single_open() 只會傳兩個參數呼叫這個 callback，型別對不上；
 * 而且現在要從 ksm_procfs.c 呼叫，必須拿掉 static 才有對外連結。
 */
int ksm_status_show(struct seq_file *m, void *v)
{
    u64 uptime = ksm_uptime();

    seq_printf(m, "Linux Kernel Security Monitor\n\n");
    seq_printf(m, "Version : 0.1\n");
    seq_printf(m, "Status  : Running\n");
    seq_printf(m, "Uptime  : %llu sec\n", uptime);
    seq_printf(m, "Events  : %llu\n", ksm_event_count());
    seq_printf(m, "Param Int  : %d\n", kc.param_int);
    seq_printf(m, "Param String  : %s\n", kc.param_str);

    return 0;
}
