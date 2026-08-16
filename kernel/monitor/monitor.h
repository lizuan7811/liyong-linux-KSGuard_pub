/*
* Created by root on 2026/7/27.
 */
#ifndef KSM_MONITOR_H
#define KSM_MONITOR_H

int _ksm_file_event_comsumer(void *data);

struct task_struct *ksm_file_event_comsumer(void);

void ksm_thread_stop(struct task_struct *task);

void ksm_intercept_event(struct file *file, int is_write);

int ksm_charactor_chan_init(void);

void ksm_file_event_unregister(void);

#endif /* KSM_MONITOR_H */