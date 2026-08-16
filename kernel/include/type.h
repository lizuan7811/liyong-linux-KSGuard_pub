/*
 * Created by root on 2026/7/27.
 */
#ifndef KSM_TYPE_H
#define KSM_TYPE_H

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/string.h>
#define FIFO_SIZE (PAGE_SIZE * 16)
#define MAX_PATH_LEN 256
/*
 * 修正: 原本這裡多放了 atomic64_t event_count / u64 start_time,
 * 但事件計數與啟動時間都已經是獨立的全域變數 (event_count, ksm_start_time,
 * 定義在 ksm_procfs.c),放在 context 裡只會造成兩份資料互相不同步。
 * context 目前只需要保存背景執行緒的 task_struct 指標。
 */
struct ksm_context {
    struct task_struct *monitor;
    int param_int; // 新增：儲存整數參數
    char *param_str; // 新增：儲存字串參數
};

struct file_event {
    __u32 pid;
    char comm[TASK_COMM_LEN];
    char filepath[MAX_PATH_LEN];
    int is_write;
    struct file *file;
};

struct cln_file_event {
    __u32 pid;
    char comm[TASK_COMM_LEN];
    char filepath[MAX_PATH_LEN];
    int is_write;
};

#endif /* KSM_TYPE_H */
