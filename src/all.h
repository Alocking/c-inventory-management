/**
 * @file all.h
 * @brief 总头文件：统一管理所有共享类型定义和函数声明
 *
 * 所有 .c 文件只需 #include "all.h" 即可，避免类型重复定义和函数声明分散。
 * 原 product.h / inventory.h / order.h / avl.h / storage.h 已清空为兼容入口，
 * 内部仅 #include "all.h"。
 */
#ifndef ALL_H
#define ALL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 *                共享类型定义（全项目唯一一份）
 * ============================================================ */

/* 商品基础业务数据 */
typedef struct {
    char sku[32];      /* 商品唯一编号 */
    char name[64];     /* 商品名称 */
    int  stock;        /* 当前库存数量 */
} ProductData;

/* 双向循环链表节点 */
typedef struct Product {
    ProductData  data;
    struct Product *prev;
    struct Product *next;
} Product;

/* AVL 平衡二叉树索引节点（以 SKU 为键） */
typedef struct AVLNode {
    char sku[32];
    Product *product;        /* 指向链表中的真实商品节点 */
    int height;
    struct AVLNode *left;
    struct AVLNode *right;
} AVLNode;

/* 商品容器：链表哨兵头 + AVL 树根 */
typedef struct {
    Product *head;
    AVLNode *avl_root;
    int count;
} ProductList;

/* 库存单据 */
typedef struct {
    char order_no[32];  /* 单据编号 */
    char sku[32];       /* 操作的商品 SKU */
    int  qty;           /* 出入库数量 */
    int  type;          /* 1=入库 2=出库 */
} StockOrder;

/* 队列节点（用于出队时动态返回） */
typedef struct QNode {
    StockOrder data;
    struct QNode *next;
} QNode;

/* 单据循环队列（数组实现） */
typedef struct {
    StockOrder items[256];
    int front;
    int rear;
    int size;
} OrderQueue;

/* 撤销操作栈（数组实现） */
typedef struct {
    StockOrder items[64];
    int top;            /* -1 表示空栈 */
} UndoStack;

/* 单据类型枚举 */
typedef enum {
    ORDER_TYPE_IN  = 1,
    ORDER_TYPE_OUT = 2
} OrderType;

/* ============================================================
 *                     AVL 接口
 * ============================================================ */
AVLNode* avl_create_node(Product *p, const char *sku);
AVLNode* avl_insert(AVLNode *root, AVLNode *node);
AVLNode* avl_delete(AVLNode *root, const char *sku);
AVLNode* sched_search(AVLNode *root, const char *sku, int flag);  /* flag!=0 时打印访问轨迹 */
void     avl_destroy(AVLNode *root);

/* ============================================================
 *                  商品链表接口
 * ============================================================ */
ProductList* list_create(void);
void     list_destroy(ProductList *list);
int      list_add(ProductList *list, Product *node);
int      list_remove(ProductList *list, char *id);
int      list_update(ProductList *list, char *id, const char *new_name);
Product* list_find(ProductList *list, char *id);
int      list_get_count(ProductList *list);
Product* list_get_head(ProductList *list);
Product* product_create(const char *sku, const char *name, int stock);

/* ============================================================
 *                  库存操作接口
 * ============================================================ */
void  stack_init(UndoStack *st);
int   stack_is_empty(const UndoStack *st);
StockOrder* stack_pop(UndoStack *st);
int   stack_push(UndoStack *st, StockOrder *ord);
void  gen_order_no(char *buf, int type);
int   stock_in (AVLNode **avl_root, OrderQueue *q, UndoStack *undo, const char *sku, int num);
int   stock_out(AVLNode **avl_root, OrderQueue *q, UndoStack *undo, const char *sku, int num);
int   stock_undo(AVLNode **avl_root, OrderQueue *q, UndoStack *undo);

/* ============================================================
 *                  单据队列接口
 * ============================================================ */
void  queue_init(OrderQueue *queue);
int   queue_is_empty(OrderQueue *queue);
int   queue_is_full(OrderQueue *queue);
int   queue_enqueue(OrderQueue *queue, StockOrder *ord);  /* 直接收 StockOrder* */
QNode* queue_dequeue(OrderQueue *queue);
const StockOrder* queue_peek(OrderQueue *queue);
void  queue_print_all(OrderQueue *queue);
QNode* create_order_node(const char *sku, int qty, int type);
void  destroy_order_node(QNode *node);

/* ============================================================
 *                  文件存储接口
 * ============================================================ */
int list_save(ProductList *list);
int list_load(ProductList *list);
int order_save(OrderQueue *queue);
int order_load(OrderQueue *queue);

#endif /* ALL_H */
