/*
 * Created by root on 2026/7/27.
 */
#ifndef KSM_STATUS_H
#define KSM_STATUS_H

#include <linux/seq_file.h>

/*
 * 簽名必須完全符合 single_open() 期望的 callback 型別：
 * int (*)(struct seq_file *, void *)
 * 原本多帶了一個 struct ksm_context *kc 參數，型別就對不上了。
 */
int ksm_status_show(struct seq_file *m, void *v);

#endif /* KSM_STATUS_H */
