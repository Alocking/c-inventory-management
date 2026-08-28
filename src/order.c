#include "all.h"

/* ============================================================
 * 循环队列操作（基于数组的环形 FIFO 队列）
 * ============================================================ */

void queue_init(OrderQueue *queue)
{
    if (queue == NULL) return;
    queue->front = 0;
    queue->rear  = 0;
    queue->size  = 0;
}

int queue_is_empty(OrderQueue *queue)
{
    return (queue == NULL) ? 1 : (queue->size == 0);
}

int queue_is_full(OrderQueue *queue)
{
    return (queue == NULL) ? 1 : (queue->size == 256);
}

// 入队：将单据拷贝到 rear 位置，更新 rear 和 size
int queue_enqueue(OrderQueue *queue, StockOrder *ord)
{
    if (queue == NULL || ord == NULL) {
        fprintf(stderr, "[错误] 入队失败：参数无效（队列或单据为空）\n");
        return -1;
    }
    if (queue_is_full(queue)) {
        fprintf(stderr, "[错误] 入队失败：队列已满，无法加入单据 %s\n",
                ord->order_no);
        return -1;
    }
    queue->items[queue->rear] = *ord;
    queue->rear = (queue->rear + 1) % 256;
    queue->size++;
    return 0;
}

// 出队：从 front 位置取出数据，动态创建 QNode 返回
// 注意：调用方负责 free 返回的 QNode*
QNode* queue_dequeue(OrderQueue *queue)
{
    if (queue == NULL) {
        fprintf(stderr, "[错误] 出队失败：队列指针为空\n");
        return NULL;
    }
    if (queue_is_empty(queue)) {
        fprintf(stderr, "[错误] 出队失败：队列为空\n");
        return NULL;
    }
    QNode *node = (QNode*)malloc(sizeof(QNode));
    if (node == NULL) {
        fprintf(stderr, "[错误] 出队失败：内存分配失败\n");
        return NULL;
    }
    node->data = queue->items[queue->front];
    node->next = NULL;
    queue->front = (queue->front + 1) % 256;
    queue->size--;
    return node;
}

// 查看队首（不移除），返回队首单据的只读指针
const StockOrder* queue_peek(OrderQueue *queue)
{
    if (queue == NULL || queue_is_empty(queue)) return NULL;
    return &queue->items[queue->front];
}

// 打印所有待处理单据（从 front 到 rear，FIFO 顺序）
void queue_print_all(OrderQueue *queue)
{
    if (queue == NULL) {
        printf("队列指针为空。\n");
        return;
    }
    if (queue_is_empty(queue)) {
        printf("暂无待处理单据。\n");
        return;
    }
    printf("========== 待处理单据（FIFO） ==========\n");
    int count = 0;
    int i = queue->front;
    while (count < queue->size) {
        const char *type_str = (queue->items[i].type == ORDER_TYPE_IN)
                               ? "入库" : "出库";
        printf("[%d] 单号: %s | SKU: %s | 数量: %d | 类型: %s\n",
               count + 1, queue->items[i].order_no,
               queue->items[i].sku, queue->items[i].qty, type_str);
        i = (i + 1) % 256;
        count++;
    }
    printf("==========================================\n");
}

/* ============================================================
 * 单据节点辅助（用于独立创建单据节点，不依赖队列）
 * ============================================================ */

// 创建单据节点：分配内存、填充数据、生成单号
QNode* create_order_node(const char *sku, int qty, int type)
{
    if (sku == NULL || qty <= 0) {
        fprintf(stderr, "[错误] 创建单据失败：SKU为空或数量非法\n");
        return NULL;
    }
    QNode *node = (QNode*)malloc(sizeof(QNode));
    if (node == NULL) {
        fprintf(stderr, "[错误] 创建单据失败：内存分配失败\n");
        return NULL;
    }
    strncpy(node->data.sku, sku, sizeof(node->data.sku) - 1);
    node->data.sku[sizeof(node->data.sku) - 1] = '\0';
    node->data.qty  = qty;
    node->data.type = type;
    node->next = NULL;

    // 统一使用 gen_order_no 生成单号
    gen_order_no(node->data.order_no, type);
    return node;
}

// 释放单据节点
void destroy_order_node(QNode *node)
{
    if (node != NULL) {
        free(node);
    }
}