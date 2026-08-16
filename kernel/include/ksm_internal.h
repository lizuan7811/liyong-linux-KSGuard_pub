#ifndef KSM_INTERNAL_H
#define KSM_INTERNAL_H

#include <linux/types.h>
#include <linux/atomic.h>

/* 被封鎖的執行檔名稱關鍵字（不分大小寫比對） */
#define BLOCKED_COMMAND "malware_test"

/*
 * 宣告全域變數供多個檔案存取。
 * 實際定義放在 ksm_procfs.c（event_count 的定義放在 event.c，見下方 event.h）,
 * 其他檔案一律用這份 extern 宣告，不要在其他 .c 檔再宣告同名的 static 變數！
 */
extern atomic64_t event_count;
extern u64 ksm_start_time;

/* procfs 生命週期 */
int ksm_procfs_init(void);
void ksm_procfs_exit(void);

#endif /* KSM_INTERNAL_H */
