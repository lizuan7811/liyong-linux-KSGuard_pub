cmake_minimum_required(VERSION 3.10)
project(ksm C)

set(CMAKE_C_STANDARD 11)

# ---------------------------------------------------------------------------
# 這個 CMakeLists.txt 不會「直接」編出 ksm.ko。
# Linux kernel module 一定要透過 kernel 原始碼樹裡的 Kbuild 系統來編譯
# （也就是專案裡的 Makefile: make -C $(KDIR) M=$(MDIR) modules），
# 一般的 gcc / cmake 編譯流程無法產生正確的 kernel module。
#
# 修正: 實際的 .c/.h 原始碼是放在專案根目錄下的 kernel/ 資料夾裡
# （不是跟 CMakeLists.txt 同一層），所以底下所有路徑都要多加上
# ${CMAKE_SOURCE_DIR}/kernel 這一層前綴。
#
# 這份檔案的用途分成兩塊：
#   1. 建立一個 EXCLUDE_FROM_ALL 的假 library target，只是讓 CLion / VSCode
#      這類 IDE 能吃到正確的 include path、巨集定義，藉此做語法高亮、
#      跳轉定義、自動補全 —— 這個 target 本身不會真的被建置。
#   2. 用 add_custom_target 呼叫既有的 Kbuild Makefile，讓你在 IDE 裡
#      按下 build 時，實際上是去執行 `make`。
# ---------------------------------------------------------------------------

# 真正的 kernel module 原始碼樹
set(KSM_SRC_DIR ${CMAKE_SOURCE_DIR}/kernel)

# 找出目前的 kernel headers 路徑，可用 -DKDIR=... 覆寫
if(NOT DEFINED KDIR)
    execute_process(
        COMMAND uname -r
        OUTPUT_VARIABLE KERNEL_RELEASE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(KDIR "/lib/modules/${KERNEL_RELEASE}/build")
endif()

if(NOT EXISTS ${KDIR})
    message(WARNING "找不到 kernel headers: ${KDIR}\n"
                     "請安裝 linux-headers-$(uname -r)，或用 -DKDIR=/path/to/kernel/build 指定路徑")
endif()

if(NOT EXISTS ${KSM_SRC_DIR})
    message(FATAL_ERROR "找不到原始碼樹: ${KSM_SRC_DIR}\n"
                         "請確認 .c/.h 檔案都放在專案根目錄下的 kernel/ 資料夾裡")
endif()

# kernel 原始碼樹裡常見會用到的 include 路徑（依平台可能需要調整 arch/x86）
set(KERNEL_INCLUDE_DIRS
    ${KDIR}/include
    ${KDIR}/include/uapi
    ${KDIR}/include/generated/uapi
    ${KDIR}/arch/x86/include
    ${KDIR}/arch/x86/include/generated
    ${KDIR}/arch/x86/include/uapi
    ${KDIR}/arch/x86/include/generated/uapi
)

# 專案自己的 include 路徑（對應各檔案裡的 ../include, ../event ... 相對路徑，
# 都是相對於 kernel/ 底下的各子資料夾）
set(KSM_INCLUDE_DIRS
    ${KSM_SRC_DIR}/include
    ${KSM_SRC_DIR}/event
    ${KSM_SRC_DIR}/time
    ${KSM_SRC_DIR}/proc
    ${KSM_SRC_DIR}/monitor
)

include_directories(SYSTEM ${KERNEL_INCLUDE_DIRS})
include_directories(${KSM_INCLUDE_DIRS})

add_definitions(
    -D__KERNEL__
    -DMODULE
    -DKBUILD_MODNAME="ksm"
)

file(GLOB_RECURSE KSM_SOURCES
    ${KSM_SRC_DIR}/core/*.c
    ${KSM_SRC_DIR}/event/*.c
    ${KSM_SRC_DIR}/time/*.c
    ${KSM_SRC_DIR}/proc/*.c
    ${KSM_SRC_DIR}/monitor/*.c
)
file(GLOB_RECURSE KSM_HEADERS
    ${KSM_SRC_DIR}/include/*.h
    ${KSM_SRC_DIR}/event/*.h
    ${KSM_SRC_DIR}/time/*.h
    ${KSM_SRC_DIR}/proc/*.h
    ${KSM_SRC_DIR}/monitor/*.h
)

# 假的 target：只給 IDE 索引用，不參與 `cmake --build .` 的預設 all target
add_library(ksm_ide_index EXCLUDE_FROM_ALL ${KSM_SOURCES} ${KSM_HEADERS}
        kernel/monitor/ksm_kprobe.c
        kernel/monitor/ksm_kprobe.h)

# 真正的建置：呼叫 Kbuild Makefile，M= 指向 kernel/ 子目錄
add_custom_target(module ALL
    COMMAND $(MAKE) -C ${KDIR} M=${KSM_SRC_DIR} modules
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building ksm.ko via Kbuild (${KDIR}), source in ${KSM_SRC_DIR}"
    VERBATIM
)

add_custom_target(module_clean
    COMMAND $(MAKE) -C ${KDIR} M=${KSM_SRC_DIR} clean
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Cleaning ksm.ko build artifacts"
    VERBATIM
)

add_custom_target(module_install
    COMMAND $(MAKE) -C ${KDIR} M=${KSM_SRC_DIR} modules_install
    COMMAND depmod -a
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Installing ksm.ko"
    VERBATIM
)
