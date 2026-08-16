#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include "../include/type.h"
#include "../include/ksm_internal.h"

static int param_int = 42;
static char *param_str = "default_str";
extern struct ksm_context kc;

static int __init ksm_init(void) {
    kc.param_int = param_int;
    kc.param_str = param_str;
    int ret = ksm_procfs_init();

    if (ret) {
        pr_err("KSM: init failed (%d)\n", ret);
        return ret;
    }

    pr_info("KSM loaded\n");
    return 0;
}

static void __exit ksm_exit(void) {
    ksm_procfs_exit();
    pr_info("KSM unloaded\n");
}

module_init(ksm_init);
module_exit(ksm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("liyong");
MODULE_DESCRIPTION("Linux Kernel Security Monitor");

module_param(param_int, int, S_IRUGO);
MODULE_PARM_DESC(param_int, "An Integer Parameter");

module_param(param_str, charp, S_IRUGO);
MODULE_PARM_DESC(param_str, "An Character string Parameter");
