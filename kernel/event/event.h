/*
 * Created by root on 2026/7/27.
 */
#ifndef KSM_EVENT_H
#define KSM_EVENT_H

#include <linux/types.h>

/*
 * 修正: 原本 ksm_event_init() 宣告成 void 但定義卻回傳 atomic64_t，
 * ksm_event_count() 宣告是 (void) 但定義卻多吃一個 ksm_context*，
 * 兩邊對不起來。這裡統一用全域 event_count（定義在 event.c），
 * 所有函式都不需要外部傳入 context。
 */
void ksm_event_init(void);
void ksm_event_inc(void);
u64 ksm_event_count(void);

#endif /* KSM_EVENT_H */
