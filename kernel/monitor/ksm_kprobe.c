#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/sched.h>
#include "ksm_kprobe.h"
#include "monitor.h"


struct kprobe ksm_open_kprobe;


static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    struct file *file;

    file = (struct file *)regs_get_kernel_argument(regs, 0);
    if (!file)
        return 0;

    /* 只關心一般檔案,socket/pipe/anon-inode 一律跳過 */
    if (!file->f_path.dentry || !file->f_path.mnt)
        return 0;
    if (!d_is_reg(file->f_path.dentry))
        return 0;

    get_file(file);
    /* 過濾（should_monitor）現在都在 ksm_intercept_event 裡提早做，
     * 不符合規則的事件會在那邊直接 fput() 丟掉，這裡不用管。 */
    ksm_intercept_event(file, 0);

    return 0;
}


int ksm_file_evnet_producer(void)
{
    memset(
        &ksm_open_kprobe,
        0,
        sizeof(ksm_open_kprobe)
    );

    ksm_open_kprobe.symbol_name =
        "security_file_open";

    ksm_open_kprobe.pre_handler =
        handler_pre;

    return register_kprobe(
        &ksm_open_kprobe
    );
}


void ksm_unregister_kprobe(void)
{
    unregister_kprobe(
        &ksm_open_kprobe
    );
}