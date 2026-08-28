#include "all.h"

/* ============================================================
 * 撤销栈操作
 * ============================================================ */

// 检查栈是否为空：空栈返回 1，非空返回 0
int stack_is_empty(const UndoStack *st)
{
    if (st == NULL || st->top == -1) {
        return 1;
    }
    return 0;
}

// 撤销栈初始化：top 置 -1 表示空栈
void stack_init(UndoStack *st)
{
    if (st != NULL) {
        st->top = -1;
    }
}

// 弹出栈顶数据（返回栈顶元素指针，调用方可读取但不应 free）
StockOrder* stack_pop(UndoStack *st)
{
    if (stack_is_empty(st)) {
        printf("撤销栈为空！\n");
        return NULL;
    }

    StockOrder *p = &st->items[st->top];
    st->top -= 1;
    return p;
}

// 压栈：将单据存入撤销栈
int stack_push(UndoStack *st, StockOrder *ord)
{
    if (st == NULL || ord == NULL) {
        printf("撤销栈或单据指针为空！\n");
        return -1;
    }
    if (st->top >= 63) {
        printf("撤销栈已满，无法继续记录操作！\n");
        return -1;
    }

    st->top += 1;
    st->items[st->top] = *ord;
    return 0;
}

/* ============================================================
 * 单号生成（统一入口，替代原 gen_order_no + generate_order_no）
 * ============================================================ */

// 生成唯一单号：IN + 5位随机数 或 OUT + 5位随机数
void gen_order_no(char *buf, int type)
{
    if (buf == NULL) return;

    int num = rand() % 90000 + 10000;  // 10000~99999，避免前导零
    if (type == ORDER_TYPE_IN) {
        sprintf(buf, "IN%d", num);
    } else if (type == ORDER_TYPE_OUT) {
        sprintf(buf, "OUT%d", num);
    } else {
        sprintf(buf, "ORD%d", num);
    }
}

/* ============================================================
 * 队列辅助：从队列中移除指定单号的单据（用于撤销操作）
 *
 * 做法：把队列所有元素逐个出队到临时数组，跳过目标单据，
 *       其余再重新入队。这样保持 FIFO 顺序不变。
 * ============================================================ */
static int queue_remove_by_order_no(OrderQueue *q, const char *order_no)
{
    if (q == NULL || order_no == NULL) {
        return -1;
    }
    if (queue_is_empty(q)) {
        return -1;
    }

    // 临时数组暂存未被移除的单据
    StockOrder temp[256];
    int temp_count = 0;
    int found = 0;

    // 逐个出队
    while (!queue_is_empty(q)) {
        QNode *node = queue_dequeue(q);
        if (node == NULL) break;

        if (strcmp(node->data.order_no, order_no) == 0) {
            // 找到目标单据，跳过（不放入临时数组）
            found = 1;
        } else {
            // 保留
            if (temp_count < 256) {
                temp[temp_count] = node->data;
                temp_count++;
            }
        }
        free(node);  // 🔧 修复：释放 QNode 内存，防止泄漏
    }

    // 将保留的单据重新入队（保持原 FIFO 顺序）
    for (int i = 0; i < temp_count; i++) {
        queue_enqueue(q, &temp[i]);
    }

    return found ? 0 : -1;
}

/* ============================================================
 * 库存操作核心实现
 * ============================================================ */

// 内部辅助：将单据写入历史队列 + 撤销栈
static void StockOrder_insert(StockOrder *ord, OrderQueue *q,
                              UndoStack *undo, const char *sku,
                              int num, int type)
{
    gen_order_no(ord->order_no, type);
    strncpy(ord->sku, sku, sizeof(ord->sku) - 1);
    ord->sku[sizeof(ord->sku) - 1] = '\0';
    ord->qty = num;
    ord->type = type;

    // 单据入历史 FIFO 队列
    if (q != NULL) {
        queue_enqueue(q, ord);
    }

    // 压入撤销栈（LIFO，用于撤销最近操作）
    stack_push(undo, ord);
}

// 采购入库：增加商品库存，生成入库单据
int stock_in(AVLNode **avl_root, OrderQueue *q, UndoStack *undo,
             const char *sku, int num)
{
    if (num <= 0 || sku == NULL || avl_root == NULL) {
        return -1;
    }

    // 🔧 修复：不再检查 *avl_root 是否为空，让 sched_search 自然返回 NULL
    AVLNode *node = sched_search(*avl_root, sku, 0);
    if (node == NULL) {
        printf("入库失败：未找到 SKU %s 对应的商品。\n", sku);
        return -1;
    }

    // 增加库存
    node->product->data.stock += num;

    // 生成单据并入队/入栈
    StockOrder ord;
    StockOrder_insert(&ord, q, undo, sku, num, ORDER_TYPE_IN);

    return 0;
}

// 销售出库：减少商品库存（库存不足时拒绝），生成出库单据
int stock_out(AVLNode **avl_root, OrderQueue *q, UndoStack *undo,
              const char *sku, int num)
{
    if (num <= 0 || sku == NULL || avl_root == NULL) {
        return -1;
    }

    // 🔧 修复：不再检查 *avl_root 是否为空
    AVLNode *node = sched_search(*avl_root, sku, 0);
    if (node == NULL) {
        printf("出库失败：未找到 SKU %s 对应的商品。\n", sku);
        return -1;
    }

    // 检查库存是否充足
    if (node->product->data.stock < num) {
        printf("出库失败：SKU %s 库存不足（当前 %d，需要 %d）。\n",
               sku, node->product->data.stock, num);
        return -2;
    }

    // 扣减库存
    node->product->data.stock -= num;

    // 生成单据并入队/入栈
    StockOrder ord;
    StockOrder_insert(&ord, q, undo, sku, num, ORDER_TYPE_OUT);

    return 0;
}

// 撤销最近一次入/出库操作
int stock_undo(AVLNode **avl_root, OrderQueue *q, UndoStack *undo)
{
    // 1. 检查撤销栈是否为空
    if (stack_is_empty(undo)) {
        printf("撤销失败：撤销栈为空，没有可撤销的操作。\n");
        return -1;
    }

    // 2. 从撤销栈弹出最近操作（LIFO）
    StockOrder *ord = stack_pop(undo);
    if (ord == NULL) {
        printf("撤销失败：撤销栈弹出异常。\n");
        return -1;
    }

    // 3. 查找对应商品
    AVLNode *node = sched_search(*avl_root, ord->sku, 0);
    if (node == NULL) {
        printf("撤销失败：未找到 SKU %s 对应的商品。\n", ord->sku);
        // 注意：已从撤销栈弹出，此处无法回滚；但这种情况不应发生
        return -1;
    }

    // 4. 反向更新库存：入库撤销则减，出库撤销则加
    if (ord->type == ORDER_TYPE_IN) {
        node->product->data.stock -= ord->qty;
    } else if (ord->type == ORDER_TYPE_OUT) {
        node->product->data.stock += ord->qty;
    }

    // 5. 🔧 修复：从 FIFO 队列中移除被撤销的那一张单据
    //    之前错误地使用 queue_dequeue（弹出队首最旧元素），
    //    现在改为按单号查找并精确移除，保持 FIFO 语义一致。
    if (q != NULL) {
        if (queue_remove_by_order_no(q, ord->order_no) != 0) {
            printf("警告：单据 %s 不在队列中（可能已被处理）。\n",
                   ord->order_no);
        }
    }

    return 0;
}