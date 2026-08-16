/*
 * Created by root on 2026/7/27.
 */
#include <linux/sched.h>
#include <linux/kfifo.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/file.h>       /* fput(), struct file */
#include <linux/fs.h>
#include <linux/dcache.h>     /* d_path() */
#include <linux/err.h>        /* IS_ERR()/IS_ERR_OR_NULL() */
#include <linux/uaccess.h>    /* kfifo_to_user 需要 */
#include <linux/compiler.h>
#include <linux/string.h>     /* strncmp/strscpy */
#include <linux/miscdevice.h>

#include "../include/type.h"
#include "monitor.h"
#include "../include/ksm_internal.h"

static bool ksm_hook_registered;

/**
 *  ksm_wq, ksm_user_wq 裡面存的是「等待中的 task」，不是 FIFO 資料。你可以把它想成 Linux kernel 裡的「門鈴」，不是「信箱」。
| 用途                | FIFO            | wait queue    |
| ----------------- | --------------- | ------------- |
| kernel event 暫存   | `ksm_fifo`      | `ksm_wq`      |
| 給 user space read | `ksm_user_fifo` | `ksm_user_wq` |
**/
static DECLARE_KFIFO(
    ksm_fifo,
    struct file_event,
    4096);

static DEFINE_SPINLOCK(ksm_lock);

static DECLARE_KFIFO(ksm_user_fifo,
                     struct cln_file_event,
                     4096);

static DEFINE_SPINLOCK(ksm_user_lock);

/* 給 monitor thread 內部消費 ksm_fifo 用的 wait queue（如果你已經有這行就不用重複加） */
static DECLARE_WAIT_QUEUE_HEAD(ksm_wq);

/* 新增：給 user-space read()/poll() 等待 ksm_user_fifo 用的 wait queue，
 * 跟上面 ksm_wq 是兩個獨立的 queue，不要共用，否則 monitor thread
 * 內部消費跟 user read() 會互相搶著被喚醒，語意會錯亂。 */
static DECLARE_WAIT_QUEUE_HEAD(ksm_user_wq);

static atomic_t dropped_events_count =
        ATOMIC_INIT(0);

static atomic_t total_captured_count =
        ATOMIC_INIT(0);

static atomic_t filtered_out_count =
        ATOMIC_INIT(0);

/* forward declaration，函式本體留在原本後面的位置就好 */
static bool should_monitor(const char *filepath, const char *comm);

/*
 * 重要提醒（設計層級的限制，不是 bug 能修完的事）：
 * mainline kernel 只允許「靜態編譯進 kernel 的 LSM」在 security_init()
 * 階段呼叫 security_add_hooks()。從一個用 insmod 載入的模組去呼叫，
 * 在標準 kernel 上通常會編譯/連結失敗，或者被安全機制擋下。
 * 這裡保留呼叫是讓程式邏輯正確、可讀，但要在真實環境動態掛 LSM hook，
 * 建議改用 kprobe/fprobe 或 eBPF LSM 等替代方案。
 */
static void ksm_lsm_register_once(void) {
    if (ksm_hook_registered)
        return;

    // security_add_hooks(my_hooks, ARRAY_SIZE(my_hooks), "ksm");
    ksm_hook_registered = true;

    pr_info("KSM: hook registered, blocking '%s'\n", BLOCKED_COMMAND);
}

/*
 * 修正 (系統卡死根因之二):
 * monitor thread 每圈只 kfifo_get() 一筆就結束該圈，被喚醒後改成用
 * 內層 for(;;) 把目前佇列裡的事件一次排空，避免事件堆積時還要
 * 一次一次睡醒/睡著切換context。
 */
int _ksm_file_event_comsumer(void *data) {
    pr_info("KSM thread started\n");

    /* 只註冊一次，不要放進 while 迴圈裡重複註冊 */
    ksm_lsm_register_once();

    while (!kthread_should_stop()) {
        bool has_event;
        unsigned long flags;
        struct file_event ev;

        // 睡在 ksm_wq 上，直到 ksm_fifo 裡面有資料。
        wait_event_interruptible_timeout(
            ksm_wq,
            !kfifo_is_empty(&ksm_fifo) || kthread_should_stop(),
            HZ);

        if (kthread_should_stop())
            break;

        for (;;) {
            spin_lock_irqsave(&ksm_lock, flags);
            has_event = kfifo_get(&ksm_fifo, &ev);
            spin_unlock_irqrestore(&ksm_lock, flags);

            if (!has_event)
                break;

            {
                char buffer[MAX_PATH_LEN];
                char *path = NULL;
                struct cln_file_event clean_ev;
                unsigned long uflags;

                if (ev.file && ev.file->f_path.dentry && ev.file->f_path.mnt)
                    path = d_path(&ev.file->f_path, buffer, sizeof(buffer));

                memset(&clean_ev, 0, sizeof(clean_ev));
                clean_ev.pid = ev.pid;
                clean_ev.is_write = ev.is_write;
                memcpy(clean_ev.comm, ev.comm, sizeof(clean_ev.comm));

                if (path && !IS_ERR(path))
                    strscpy(clean_ev.filepath, path, sizeof(clean_ev.filepath));
                else
                    strscpy(clean_ev.filepath, "unknown", sizeof(clean_ev.filepath));

                /* 這裡的事件在 producer 端就已經確定是要記錄的，
                 * 不需要再跑一次 should_monitor()。 */
                fput(ev.file);

                pr_info("KSM pid=%d file=%s\n", clean_ev.pid, clean_ev.filepath);

                spin_lock_irqsave(&ksm_user_lock, uflags);
                bool ok = kfifo_put(&ksm_user_fifo, clean_ev);
                spin_unlock_irqrestore(&ksm_user_lock, uflags);

                if (!ok) {
                    pr_warn_ratelimited("KSM: user fifo full\n");
                } else {
                    wake_up_interruptible(&ksm_user_wq);
                }
            }
        }
    }
    return 0;
}

static ssize_t ksm_dev_read(struct file *filp,
                            char __user *buffer,
                            size_t len,
                            loff_t *offset)
{
    unsigned long flags;
    size_t n_records = len / sizeof(struct cln_file_event);
    size_t copied_records = 0;
    struct cln_file_event ev;

    if (len < sizeof(struct cln_file_event))
        return -EINVAL;

    while (copied_records < n_records) {
        bool has_event;

        spin_lock_irqsave(&ksm_user_lock, flags);
        has_event = kfifo_get(&ksm_user_fifo, &ev);  /* 純 memcpy，安全，不會睡眠 */
        spin_unlock_irqrestore(&ksm_user_lock, flags);

        if (!has_event) {
            if (copied_records > 0)
                break;  /* 已經讀到至少一筆，先回傳給使用者，不要在這裡等 */

            if (filp->f_flags & O_NONBLOCK)
                return -EAGAIN;

            if (wait_event_interruptible(ksm_user_wq,
                    !kfifo_is_empty(&ksm_user_fifo)))
                return -ERESTARTSYS;

            continue;
        }

        /* 鎖已經放開了，這裡才做 copy_to_user，完全不在 atomic context 裡 */
        if (copy_to_user(buffer + copied_records * sizeof(ev),
                          &ev, sizeof(ev))) {
            return copied_records ?
                (ssize_t)(copied_records * sizeof(ev)) : -EFAULT;
                          }

        copied_records++;
    }

    return (ssize_t)(copied_records * sizeof(struct cln_file_event));
}

static const struct file_operations ksm_fifo_operations = {
    .owner = THIS_MODULE,
    .read = ksm_dev_read,
};

static struct miscdevice ksm_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "ksm_dev",
    .fops = &ksm_fifo_operations,
};

struct task_struct *ksm_file_event_comsumer(void) {
    return kthread_run(_ksm_file_event_comsumer, NULL, "ksm_monitor");
}

/*
 * kthread_stop() 會阻塞到該 thread 函式真正 return 為止，之後才能保證
 * 自己是唯一的消費者，才能安全排乾 ksm_fifo。這點跟原本的分析一樣，
 * 沒有變。
 */
void ksm_thread_stop(struct task_struct *task) {
    struct file_event ev;

    if (task)
        kthread_stop(task);

    while (1) {
        unsigned long flags;
        bool ok;

        spin_lock_irqsave(&ksm_lock, flags);
        ok = kfifo_get(&ksm_fifo, &ev);
        spin_unlock_irqrestore(&ksm_lock, flags);

        if (!ok)
            break;

        fput(ev.file);
    }
}

/*
 * 監控規則清單。回傳 true 表示這個 filepath 要被記錄。
 */
static bool should_monitor(const char *filepath, const char *comm) {
    static const char *const watch_prefixes[] = {
        "/tmp/",
        "/etc/",
        "/var/spool/cron/",
        "/etc/cron.d/",
        "/etc/systemd/system/",
        "/lib/systemd/system/",
        "/root/.ssh/",
        "/home/",
        "/var/www/",
        "/dev/shm/",
    };
    size_t i;

    if (!filepath)
        return false;

    for (i = 0; i < ARRAY_SIZE(watch_prefixes); i++) {
        if (strncmp(filepath, watch_prefixes[i],
                    strlen(watch_prefixes[i])) == 0)
            return true;
    }

    if (strcmp(comm, "systemd") == 0)
        return true;

    return false;
}

/*
 * 修正 (系統卡死根因之一，最關鍵的一處):
 * 原本這裡完全不做過濾，把「每一個」security_file_open 事件（幾乎等於
 * 每一次 open()）不分青紅皂白塞進 ksm_fifo，等 monitor thread 之後才
 * 用 should_monitor() 判斷要不要丟棄。但 thread 端本來就被限速在很低
 * 的消費速度，一旦系統瞬間 open 量超過消費上限，fifo 很快被灌爆，
 * 之後每個 open 都要多付出一次 get_file()+kfifo_put 失敗+fput() 的
 * 成本，整台機器就會被拖到看起來像卡死。
 *
 * 修正方式：把過濾（d_path + should_monitor）提前到 producer 端做。
 * 不符合規則的事件在這裡就直接 fput() 丟掉，完全不進 fifo，
 * 也不會佔用 monitor thread 的時間；真正塞進 fifo 的事件量會大幅降低，
 * 使得原本的「熱路徑量」跟「消費上限」不再是對立關係。
 */
void ksm_intercept_event(struct file *file, int is_write) {
    struct file_event ev;
    char pathbuf[MAX_PATH_LEN];
    char comm[TASK_COMM_LEN];
    char *path;
    unsigned long flags;

    if (!file)
        return;

    get_task_comm(comm, current);

    path = NULL;
    if (file->f_path.dentry && file->f_path.mnt)
        path = d_path(&file->f_path, pathbuf, sizeof(pathbuf));

    if (IS_ERR_OR_NULL(path))
        path = "unknown";

    if (!should_monitor(path, comm)) {
        atomic_inc(&filtered_out_count);
        fput(file);
        return;
    }

    memset(&ev, 0, sizeof(ev));
    ev.pid = current->pid;
    memcpy(ev.comm, comm, sizeof(ev.comm));
    ev.file = file;
    ev.is_write = is_write;

    spin_lock_irqsave(&ksm_lock, flags);
    if (!kfifo_put(&ksm_fifo, ev)) {
        atomic_inc(&dropped_events_count);
        spin_unlock_irqrestore(&ksm_lock, flags);
        fput(file);
        return;
    }
    atomic_inc(&total_captured_count);
    spin_unlock_irqrestore(&ksm_lock, flags);

    /* 有事件進來，馬上叫醒 thread，不要讓它睡在固定週期裡等 */
    wake_up_interruptible(&ksm_wq);
}

int ksm_charactor_chan_init(void)
{
    pr_info("KSM: sizeof(file_event)=%zu\n",
            sizeof(struct file_event));

    pr_info("KSM: sizeof(cln_file_event)=%zu\n",
            sizeof(struct cln_file_event));

    pr_info("KSM: ksm_fifo size=%u\n",
            kfifo_size(&ksm_fifo));

    pr_info("KSM: ksm_user_fifo size=%u\n",
            kfifo_size(&ksm_user_fifo));

    return misc_register(&ksm_misc_device);
}

void ksm_file_event_unregister(void) {
    misc_deregister(&ksm_misc_device);
}