#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/ktime.h>
#include <linux/atomic.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/kthread.h>

#include "../include/ksm_internal.h"
#include "../include/type.h"
#include "../time/uptime.h"
#include "../proc/status.h"
#include "../event/event.h"
#include "../monitor/monitor.h"
#include "../monitor/ksm_kprobe.h"

static struct proc_dir_entry *ksm_proc_entry;
extern struct kprobe ksm_open_kprobe;

/*
 * 這裡是 kc / ksm_start_time 唯一的「定義」處，其他檔案一律用 extern 引用。
 * (event_count 的定義在 event.c，見 ksm_internal.h 的 extern 宣告)
 */
struct ksm_context kc;
u64 ksm_start_time;

static int ksm_status_open(struct inode *inode, struct file *file) {
    return single_open(file, ksm_status_show, NULL);
}

static const struct proc_ops ksm_proc_ops = {
    .proc_open = ksm_status_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

int ksm_procfs_init(void) {
    int ret;
    int probe_ret;
    int fileevent_ret;

    ksm_event_init();

    ksm_start_time = ktime_get_seconds();

    ksm_proc_entry = proc_create(
        "ksm_status",
        0444,
        NULL,
        &ksm_proc_ops);

    if (!ksm_proc_entry) {
        pr_err("KSM: failed to create /proc/ksm_status\n");
        return -ENOMEM;
    }

    // Initialize KSM event processing thread to consume file events from ksm_wq
    kc.monitor = ksm_file_event_comsumer();

    if (IS_ERR(kc.monitor)) {
        ret = PTR_ERR(kc.monitor);

        pr_err("KSM: failed to create ksm_monitor_task\n");
        kc.monitor = NULL;
        proc_remove(ksm_proc_entry);
        ksm_proc_entry = NULL;
        return ret;
    }

    // mount misc device (/dev/ksm_dev) and register
    fileevent_ret = ksm_charactor_chan_init();

    if (fileevent_ret < 0) {
        pr_err("KSM file event_ret failed %d\n", fileevent_ret);

        /*
         * 修正: 原本這裡在 misc_register() 失敗之後，又呼叫
         * ksm_file_event_unregister()（內部是 misc_deregister()），
         * 對一個根本沒註冊成功的 misc device 做 deregister 是未定義
         * 行為，可能造成後續操作 crash。misc_register 失敗代表
         * device 從沒建立過，這裡不該再呼叫 unregister。
         *
         * 另外原本這裡直接把 kc.monitor 設成 NULL，卻沒有先
         * kthread_stop() 真正停掉那個已經在跑的 monitor thread，
         * 會讓 thread 變成孤兒，之後 rmmod 也清不掉它，
         * 只能重開機。改成跟下面 kprobe 失敗分支一樣，
         * 先用 ksm_thread_stop() 正確停止再清 proc entry。
         */
        ksm_thread_stop(kc.monitor);
        kc.monitor = NULL;

        proc_remove(ksm_proc_entry);
        ksm_proc_entry = NULL;

        return fileevent_ret;
    }

    // initial and start kprobe, implement handler_pre to produce file event to ksm_wq
    probe_ret = ksm_file_evnet_producer();

    if (probe_ret < 0) {
        pr_err("KSM kprobe failed %d\n", probe_ret);

        ksm_thread_stop(kc.monitor);
        kc.monitor = NULL;

        ksm_file_event_unregister();

        proc_remove(ksm_proc_entry);
        ksm_proc_entry = NULL;

        return probe_ret;
    }

    pr_info("KSM: procfs initialized\n");

    return 0;
}

void ksm_procfs_exit(void) {
    pr_info("KSM: stopping\n");

    /* 1. 先停止事件來源 */
    ksm_unregister_kprobe();

    /* 2. 停 monitor thread */
    if (kc.monitor) {
        ksm_thread_stop(kc.monitor);
        kc.monitor = NULL;
    }

    /* 3. 移除 user interface */
    ksm_file_event_unregister();

    /* 4. proc */
    if (ksm_proc_entry) {
        proc_remove(ksm_proc_entry);
        ksm_proc_entry = NULL;
    }

    pr_info("KSM: unloaded\n");
}
