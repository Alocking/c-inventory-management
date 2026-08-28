/**
 * @file test_main.c
 * @brief main.c 辅助函数的单元测试（纯 assert 形式）
 *
 * 编译：make test
 * 运行：./test  或  test.exe
 *
 * 实现方式：
 *   1. 用 #define main _disabled_main 屏蔽 main.c 自带的 main 函数
 *   2. #include "main.c" 将 main.c 的辅助函数与全局变量全部引入本文件
 *   3. 本文件再提供自己的 main 函数运行所有测试用例
 *
 * 这样无需修改 main.c（保留其 static 封装），也无需 link main.o。
 * 测试用例直接调用 print_menu / load_all_data / save_all_data / save_and_cleanup。
 *
 * 测试产生的 product.dat / stock_order.dat 文件每个用例前后都会清理，
 * 不会污染项目目录；测试结束后可执行 make clean 清理。
 */

/* 屏蔽 main.c 的 main 函数，避免与本测试文件的 main 冲突 */
#define main _disabled_main
#include "main.c"
#undef main

#include <assert.h>

/* 跨平台 null 设备，用于屏蔽 print_menu 输出 */
#ifdef _WIN32
  #define NULL_DEV "nul"
#else
  #define NULL_DEV "/dev/null"
#endif

/* 清理测试产生的数据文件，保证用例间相互独立 */
static void cleanup_data_files(void) {
    remove("product.dat");
    remove("stock_order.dat");
}

/* 重置 g_exited 标志，使 save_and_cleanup 可被多次调用 */
static void reset_exit_flag(void) {
    g_exited = 0;
}

/* ====================== 测试用例 ====================== */

/* 1. print_menu 不崩溃（直接调用，会有菜单文字输出，属正常现象） */
static void test_print_menu_runs(void) {
    print_menu();
}

/* 2. g_list=NULL 时 save_all_data 应返回 -1 */
static void test_save_all_data_null_list(void) {
    g_list = NULL;
    assert(save_all_data() == -1);
}

/* 3. g_list=NULL 时 load_all_data 应返回 -1 */
static void test_load_all_data_null_list(void) {
    g_list = NULL;
    assert(load_all_data() == -1);
}

/* 4. 空 list 保存后重新加载，商品数应为 0 */
static void test_save_then_load_empty(void) {
    cleanup_data_files();

    g_list = list_create();
    assert(g_list != NULL);
    assert(save_all_data() == 0);

    list_destroy(g_list);
    g_list = list_create();
    assert(g_list != NULL);

    /* list_load 在文件不存在时返回 0，存在空文件时也返回 0 */
    assert(load_all_data() == 0);
    assert(list_get_count(g_list) == 0);

    list_destroy(g_list);
    g_list = NULL;
    cleanup_data_files();
}

/* 5. 添加一个商品保存后重新加载，count 应为 1 且能 find 到，字段一致 */
static void test_save_then_load_with_product(void) {
    cleanup_data_files();

    g_list = list_create();
    assert(g_list != NULL);

    Product *p = product_create("SKU001", "测试商品", 100);
    assert(p != NULL);
    assert(list_add(g_list, p) == 0);
    assert(save_all_data() == 0);

    list_destroy(g_list);
    g_list = list_create();
    assert(g_list != NULL);

    assert(load_all_data() == 0);
    assert(list_get_count(g_list) == 1);

    Product *found = list_find(g_list, "SKU001");
    assert(found != NULL);
    assert(found->data.stock == 100);
    assert(strcmp(found->data.name, "测试商品") == 0);
    assert(strcmp(found->data.sku, "SKU001") == 0);

    list_destroy(g_list);
    g_list = NULL;
    cleanup_data_files();
}

/* 6. g_list=NULL 时 save_and_cleanup 不崩溃，且 g_list 仍为 NULL */
static void test_save_and_cleanup_with_null(void) {
    reset_exit_flag();
    g_list = NULL;
    save_and_cleanup();
    assert(g_list == NULL);
}

/* 7. 创建 list 并添加商品，调用 save_and_cleanup 后 g_list 应被置 NULL（已销毁） */
static void test_save_and_cleanup_with_list(void) {
    cleanup_data_files();
    reset_exit_flag();

    g_list = list_create();
    assert(g_list != NULL);

    Product *p = product_create("SKU002", "退出测试", 50);
    assert(list_add(g_list, p) == 0);

    save_and_cleanup();
    /* 关键断言：save_and_cleanup 必须把 g_list 置 NULL，避免悬空指针 */
    assert(g_list == NULL);

    cleanup_data_files();
}

/* ====================== 测试 main ====================== */
int main(void) {
    /* 全局状态初始化到安全值，避免 save_and_cleanup 系列访问野值 */
    queue_init(&g_order_queue);
    stack_init(&g_undo_stack);
    g_list = NULL;

    printf("==== Running tests for main.c ====\n");

    struct {
        const char *name;
        void (*fn)(void);
    } tests[] = {
        {"test_print_menu_runs",             test_print_menu_runs},
        {"test_save_all_data_null_list",     test_save_all_data_null_list},
        {"test_load_all_data_null_list",     test_load_all_data_null_list},
        {"test_save_then_load_empty",        test_save_then_load_empty},
        {"test_save_then_load_with_product", test_save_then_load_with_product},
        {"test_save_and_cleanup_with_null",  test_save_and_cleanup_with_null},
        {"test_save_and_cleanup_with_list",  test_save_and_cleanup_with_list},
    };
    int n = sizeof(tests) / sizeof(tests[0]);

    int passed = 0;
    for (int i = 0; i < n; i++) {
        printf("[%d/%d] %s ... ", i + 1, n, tests[i].name);
        tests[i].fn();
        passed++;
        printf("PASS\n");
    }

    printf("\n==== Summary: %d/%d passed ====\n", passed, n);
    if (passed != n) {
        printf("FAILED\n");
        return EXIT_FAILURE;
    }
    printf("ALL PASSED\n");
    return EXIT_SUCCESS;
}
