/**
 * @file main.c
 * @brief 进销存库存管理系统 - 主程序入口与菜单交互
 *
 * 重新实现版：
 * - 使用统一头文件 all.h
 * - 修复原 main 中 stack_init 缺失、handle_exit 签名不匹配等问题
 * - 所有被调用函数名严格对应 .c 实现的接口
 *
 * 编译：make
 * 运行：./inventory
 */

#include "all.h"
#include <signal.h>

/* ---------- 全局数据结构 ---------- */
ProductList *g_list = NULL;        /* 商品容器（链表 + AVL 树） */
OrderQueue  g_order_queue;         /* 单据队列 */
UndoStack   g_undo_stack;          /* 撤销栈 */

/* 用于避免 atexit + signal 重复触发保存 */
static volatile sig_atomic_t g_exited = 0;

/* ---------- 函数声明 ---------- */
static void print_menu(void);
static void save_and_cleanup(void);   /* 给 atexit 使用，无参 */
static void on_sigint(int sig);       /* 给 signal 使用，带参 */
static int  load_all_data(void);
static int  save_all_data(void);

/* ---------- 主函数 ---------- */
int main(void)
{
    srand((unsigned)time(NULL));

    /* 初始化商品容器 */
    g_list = list_create();
    if (g_list == NULL) {
        fprintf(stderr, "错误：初始化商品容器失败！\n");
        return EXIT_FAILURE;
    }

    /* 初始化队列与撤销栈 */
    queue_init(&g_order_queue);
    stack_init(&g_undo_stack);

    /* 注册退出钩子：atexit 用无参函数；signal 用带参函数 */
    atexit(save_and_cleanup);
    signal(SIGINT, on_sigint);

    /* 加载数据文件（若存在） */
    if (load_all_data() == 0) {
        printf("数据加载成功。\n");
    } else {
        printf("首次使用，未找到数据文件或文件损坏。\n");
    }

    /* 主循环 */
    int choice;
    char sku[32], name[64];
    int stock, qty;
    Product *p = NULL;

    while (1) {
        print_menu();
        printf("请选择操作: ");
        if (scanf("%d", &choice) != 1) {
            /* 非数字输入清空缓冲区，避免死循环 */
            int c; while ((c = getchar()) != '\n' && c != EOF) {}
            printf("无效输入，请输入数字。\n");
            continue;
        }
        getchar(); /* 吸收换行符 */

        switch (choice) {
            case 0: /* 退出 */
                printf("退出系统。\n");
                return EXIT_SUCCESS;

            case 1: { /* 新增商品 */
                printf("请输入SKU: ");
                if(fgets(sku, sizeof(sku), stdin)==NULL)break;   
                sku[strcspn(sku, "\n")] = '\0';
                printf("请输入商品名称: ");
                if(fgets(name, sizeof(name), stdin)==NULL) break;
                name[strcspn(name, "\n")] = '\0';
                printf("请输入初始库存: ");
                if (scanf("%d", &stock) != 1) { printf("库存输入无效。\n"); int c; while ((c=getchar())!='\n' && c!=EOF){} break; }
                getchar();

                Product *new_node = product_create(sku, name, stock);
                if (new_node == NULL) {
                    printf("错误：创建商品节点失败（内存不足）。\n");
                    break;
                }
                if (list_add(g_list, new_node) == 0) {
                    printf("商品添加成功。\n");
                } else {
                    printf("添加失败，SKU 可能已存在。\n");
                    free(new_node);
                }
                break;
            }

            case 2: { /* 删除商品 */
                printf("请输入要删除的SKU: ");
                if(fgets(sku, sizeof(sku), stdin)==NULL) break;
                sku[strcspn(sku, "\n")] = '\0';
                int ret = list_remove(g_list, sku);
                if (ret == 0)
                    printf("商品删除成功。\n");
                else if (ret == -2)
                    printf("删除失败：库存不为 0，请先清空库存。\n");
                else
                    printf("删除失败：SKU 不存在。\n");
                break;
            }

            case 3: { /* 修改商品名称 */
                printf("请输入要修改的SKU: ");
                if(fgets(sku, sizeof(sku), stdin)==NULL)break;
                sku[strcspn(sku, "\n")] = '\0';
                printf("请输入新的商品名称: ");
                if(fgets(name, sizeof(name), stdin)==NULL)break;
                name[strcspn(name, "\n")] = '\0';
                if (list_update(g_list, sku, name) == 0)
                    printf("商品修改成功。\n");
                else
                    printf("修改失败：SKU 不存在。\n");
                break;
            }

            case 4: { /* 查询商品 */
                printf("请输入要查询的SKU: ");
                if(fgets(sku, sizeof(sku), stdin)==NULL)break;
                sku[strcspn(sku, "\n")] = '\0';
                p = list_find(g_list, sku);
                if (p) {
                    printf("SKU: %s, 名称: %s, 库存: %d\n",
                           p->data.sku, p->data.name, p->data.stock);
                } else {
                    printf("未找到该商品。\n");
                }
                break;
            }

            case 5: { /* 采购入库 */
                printf("请输入入库SKU: ");
                if(fgets(sku, sizeof(sku), stdin)==NULL)break;
                sku[strcspn(sku, "\n")] = '\0';
                printf("请输入入库数量: ");
                if (scanf("%d", &qty) != 1) { printf("数量输入无效。\n"); int c; while ((c=getchar())!='\n' && c!=EOF){} break; }
                getchar();
                int ret = stock_in(&g_list->avl_root, &g_order_queue, &g_undo_stack, sku, qty);
                if (ret == 0)
                    printf("入库成功，单据已加入队列。\n");
                else if (ret == -1)
                    printf("入库失败：商品不存在或参数无效。\n");
                else
                    printf("入库失败：未知错误。\n");
                break;
            }

            case 6: { /* 销售出库 */
                printf("请输入出库SKU: ");
                if(fgets(sku, sizeof(sku), stdin)==NULL)break;
                sku[strcspn(sku, "\n")] = '\0';
                printf("请输入出库数量: ");
                if (scanf("%d", &qty) != 1) { printf("数量输入无效。\n"); int c; while ((c=getchar())!='\n' && c!=EOF){} break; }
                getchar();
                int ret = stock_out(&g_list->avl_root, &g_order_queue, &g_undo_stack, sku, qty);
                if (ret == 0)
                    printf("出库成功，单据已加入队列。\n");
                else if (ret == -1)
                    printf("出库失败：商品不存在或参数无效。\n");
                else if (ret == -2)
                    printf("出库失败：库存不足。\n");
                else
                    printf("出库失败：未知错误。\n");
                break;
            }

            case 7: { /* 撤销最近一次入/出库 */
                int ret = stock_undo(&g_list->avl_root, &g_order_queue, &g_undo_stack);
                if (ret == 0)
                    printf("撤销成功。\n");
                else if (ret == -1)
                    printf("撤销失败：撤销栈为空或商品不存在。\n");
                else
                    printf("撤销失败：未知错误。\n");
                break;
            }

            case 8: { /* 查看待处理单据队列 */
                queue_print_all(&g_order_queue);
                break;
            }

            case 9: { /* 保存数据 */
                if (save_all_data() == 0)
                    printf("数据保存成功。\n");
                else
                    printf("数据保存失败。\n");
                break;
            }

            case 10: { /* 加载数据 */
                if (load_all_data() == 0)
                    printf("数据加载成功。\n");
                else
                    printf("数据加载失败。\n");
                break;
            }

            case 11: { /* 商品列表 */
                printf("\n===== 全部商品列表 =====\n");
                Product *cur = g_list->head->next;
                while (cur != g_list->head)
                {
                    printf("SKU: %-30s 名称: %-40s 库存: %d\n",
                           cur->data.sku, cur->data.name, cur->data.stock);
                    cur = cur->next;
                }
                printf("========================\n");
                if(g_list->count == 0)
                {
                    printf("当前没有商品数据\n");
                }
                break;
            }

            default:
                printf("无效选择，请输入 0-11 之间的数字。\n");
                break;
        }
    }

    return EXIT_SUCCESS;
}

/* ---------- 辅助函数实现 ---------- */

static void print_menu(void)
{
    printf("\n========== 进销存库存管理系统 ==========\n");
    printf("1. 新增商品\n");
    printf("2. 删除商品\n");
    printf("3. 修改商品名称\n");
    printf("4. 查询商品（按SKU）\n");
    printf("5. 采购入库\n");
    printf("6. 销售出库\n");
    printf("7. 撤销最近操作\n");
    printf("8. 查看待处理单据队列\n");
    printf("9. 保存数据到文件\n");
    printf("10. 从文件加载数据\n");
    printf("11. 商品列表\n");
    printf("0. 退出\n");
    printf("========================================\n");
}

/**
 * @brief 加载所有数据（商品 + 订单），撤销栈不持久化
 * @return 0 成功，-1 失败
 */
static int load_all_data(void)
{
    if (g_list == NULL) return -1;

    if (list_load(g_list) != 0) {
        fprintf(stderr, "警告：商品数据加载失败或文件损坏。\n");
        return -1;
    }

    if (order_load(&g_order_queue) != 0) {
        fprintf(stderr, "警告：订单数据加载失败或文件损坏。\n");
        return -1;
    }

    /* 撤销栈仅当前会话有效，加载后清空 */
    stack_init(&g_undo_stack);
    return 0;
}

/**
 * @brief 保存所有数据（商品 + 订单）
 * @return 0 成功，-1 失败
 */
static int save_all_data(void)
{
    if (g_list == NULL) return -1;

    int r1 = list_save(g_list);
    int r2 = order_save(&g_order_queue);
    if (r1 != 0 || r2 != 0) {
        fprintf(stderr, "保存数据时发生错误。\n");
        return -1;
    }
    return 0;
}

/**
 * @brief 退出时保存并释放资源（atexit 回调，无参）
 *
 * 用 g_exited 防止 atexit 与 signal 双重触发导致重复保存。
 */
static void save_and_cleanup(void)
{
    if (g_exited) return;
    g_exited = 1;

    printf("\n正在保存数据并退出...\n");
    if (save_all_data() == 0) {
        printf("数据已保存。\n");
    } else {
        printf("保存失败，请手动检查数据文件。\n");
    }

    if (g_list != NULL) {
        list_destroy(g_list);
        g_list = NULL;
    }
}

/**
 * @brief Ctrl+C 信号处理：调用 atexit 已注册的清理逻辑后退出
 */
static void on_sigint(int sig)
{
    (void)sig;
    save_and_cleanup();
    exit(EXIT_SUCCESS);
}
