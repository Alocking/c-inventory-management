#include "all.h"

// 文件名常量
#define PRODUCT_FILE "product.dat"
#define ORDER_FILE "stock_order.dat"

// 文件魔数（4字节签名），用于校验文件格式
#define PRODUCT_MAGIC 0x49565450U  // "IVTP" (Inventory Product)
#define ORDER_MAGIC   0x4956544FU  // "IVTO" (Inventory Order)
#define FILE_VERSION  1            // 当前文件格式版本号

/* ============================================================
 * 商品文件读写（product.dat）
 * ============================================================ */

int list_save(ProductList *list)
{
    FILE *fp = NULL;
    Product *curr = NULL;
    int count = 0;
    int magic = PRODUCT_MAGIC;
    int version = FILE_VERSION;

    if (list == NULL) {
        return -1;
    }

    // 先写临时文件再 rename 替换，避免 fopen("wb") 立即截断原文件
    fp = fopen(PRODUCT_FILE ".tmp", "wb");
    if (fp == NULL) {
        fprintf(stderr, "ERROR: Failed to open product file for writing.\n");
        goto fail;
    }

    // 文件头：魔数 + 版本号 + 商品总数
    if (fwrite(&magic, sizeof(int), 1, fp) != 1) {
        fprintf(stderr, "ERROR: Failed to write product magic.\n");
        goto fail;
    }
    if (fwrite(&version, sizeof(int), 1, fp) != 1) {
        fprintf(stderr, "ERROR: Failed to write product version.\n");
        goto fail;
    }

    // 统计商品总数
    if (list->head != NULL) {
        curr = list->head->next;
        while (curr != list->head) {
            count++;
            curr = curr->next;
        }
    }

    if (fwrite(&count, sizeof(int), 1, fp) != 1) {
        fprintf(stderr, "ERROR: Failed to write product count.\n");
        goto fail;
    }

    // 循环写入每个商品的数据（仅写 ProductData，不写指针）
    if (list->head != NULL) {
        curr = list->head->next;
        while (curr != list->head) {
            if (fwrite(&(curr->data), sizeof(ProductData), 1, fp) != 1) {
                fprintf(stderr, "ERROR: Failed to write product data for SKU %s.\n",
                        curr->data.sku);
                goto fail;
            }
            curr = curr->next;
        }
    }

    fclose(fp);

    // 原子替换：写入全部成功后 rename
    if (rename(PRODUCT_FILE ".tmp", PRODUCT_FILE) != 0) {
        fprintf(stderr, "ERROR: Failed to rename product temp file.\n");
        remove(PRODUCT_FILE ".tmp");
        return -1;
    }
    return 0;

fail:
    if (fp != NULL) {
        fclose(fp);
    }
    remove(PRODUCT_FILE ".tmp");
    return -1;
}

// 从二进制文件加载商品链表
int list_load(ProductList *list)
{
    FILE *fp = NULL;
    int count = 0;
    int magic = 0;
    int version = 0;
    ProductData temp_data;
    Product *new_node = NULL;

    if (list == NULL) return -1;

    fp = fopen(PRODUCT_FILE, "rb");
    if (fp == NULL) {
        // 文件不存在视为首次运行
        return 0;
    }

    // 校验文件魔数
    if (fread(&magic, sizeof(int), 1, fp) != 1) {
        fprintf(stderr, "ERROR: Failed to read product file magic.\n");
        goto fail;
    }
    if (magic != PRODUCT_MAGIC) {
        fprintf(stderr, "ERROR: Product file corrupted (magic mismatch: 0x%08X).\n",
                magic);
        goto fail;
    }

    // 校验版本号
    if (fread(&version, sizeof(int), 1, fp) != 1) {
        fprintf(stderr, "ERROR: Failed to read product file version.\n");
        goto fail;
    }
    if (version != FILE_VERSION) {
        fprintf(stderr, "ERROR: Unsupported product file version %d (expected %d).\n",
                version, FILE_VERSION);
        goto fail;
    }

    // 读取商品总数
    if (fread(&count, sizeof(int), 1, fp) != 1) {
        fprintf(stderr, "ERROR: Failed to read product count.\n");
        goto fail;
    }

    // 校验 count 合法性
    if (count < 0 || count > 100000) {
        fprintf(stderr, "ERROR: Product count %d out of range.\n", count);
        goto fail;
    }

    // 逐个读取商品数据
    for (int i = 0; i < count; i++) {
        if (fread(&temp_data, sizeof(ProductData), 1, fp) != 1) {
            fprintf(stderr, "ERROR: Failed to read product data at index %d.\n", i);
            goto fail;
        }

        // 校验 SKU 非空
        if (temp_data.sku[0] == '\0') {
            fprintf(stderr, "ERROR: Product at index %d has empty SKU (file corrupted).\n", i);
            goto fail;
        }

        new_node = product_create(temp_data.sku, temp_data.name, temp_data.stock);
        if (new_node == NULL) {
            fprintf(stderr, "ERROR: Failed to create product node for SKU %s.\n",
                    temp_data.sku);
            goto fail;
        }

        if (list_add(list, new_node) != 0) {
            fprintf(stderr, "ERROR: Failed to add product %s to list (possibly duplicate).\n",
                    temp_data.sku);
            free(new_node);
            goto fail;
        }
    }

    fclose(fp);
    return 0;

fail:
    if (fp != NULL) {
        fclose(fp);
    }
    return -1;
}

/* ============================================================
 * 订单文件读写（stock_order.dat）
 * ============================================================ */

int order_save(OrderQueue *queue)
{
    FILE *fp = NULL;
    int count = 0;
    int magic = ORDER_MAGIC;
    int version = FILE_VERSION;

    if (queue == NULL) {
        return -1;
    }

    fp = fopen(ORDER_FILE ".tmp", "wb");
    if (fp == NULL) {
        fprintf(stderr, "ERROR: Failed to open order file for writing.\n");
        goto fail;
    }

    // 文件头：魔数 + 版本号 + 订单总数
    if (fwrite(&magic, sizeof(int), 1, fp) != 1) {
        fprintf(stderr, "ERROR: Failed to write order magic.\n");
        goto fail;
    }
    if (fwrite(&version, sizeof(int), 1, fp) != 1) {
        fprintf(stderr, "ERROR: Failed to write order version.\n");
        goto fail;
    }

    count = queue->size;
    if (fwrite(&count, sizeof(int), 1, fp) != 1) {
        fprintf(stderr, "ERROR: Failed to write order count.\n");
        goto fail;
    }

    // 队列为空直接返回（仍需 rename 原子替换）
    if (count == 0) {
        fclose(fp);
        if (rename(ORDER_FILE ".tmp", ORDER_FILE) != 0) {
            fprintf(stderr, "ERROR: Failed to rename order temp file.\n");
            remove(ORDER_FILE ".tmp");
            return -1;
        }
        return 0;
    }

    // 🔧 修复：从 front 指针开始，按循环顺序写入真实队列数据
    // 之前直接写 items[0..count-1]，在发生过撤销后 front 不再为 0，
    // 导致保存的是旧数据，加载后队列状态损坏
    int idx = queue->front;
    for (int i = 0; i < count; i++) {
        if (fwrite(&queue->items[idx], sizeof(StockOrder), 1, fp) != 1) {
            fprintf(stderr, "ERROR: Failed to write order data at index %d.\n", i);
            goto fail;
        }
        idx = (idx + 1) % 256;
    }

    fclose(fp);

    // 原子替换
    if (rename(ORDER_FILE ".tmp", ORDER_FILE) != 0) {
        fprintf(stderr, "ERROR: Failed to rename order temp file.\n");
        remove(ORDER_FILE ".tmp");
        return -1;
    }
    return 0;

fail:
    if (fp != NULL) {
        fclose(fp);
    }
    remove(ORDER_FILE ".tmp");
    return -1;
}

int order_load(OrderQueue *queue)
{
    FILE *fp = NULL;
    int count = 0;
    int magic = 0;
    int version = 0;
    int max_capacity = 0;

    if (queue == NULL) return -1;

    fp = fopen(ORDER_FILE, "rb");
    if (fp == NULL) {
        return 0;  // 文件不存在视为首次运行
    }

    // 校验文件魔数
    if (fread(&magic, sizeof(int), 1, fp) != 1) {
        fprintf(stderr, "ERROR: Failed to read order file magic.\n");
        goto fail;
    }
    if (magic != ORDER_MAGIC) {
        fprintf(stderr, "ERROR: Order file corrupted (magic mismatch: 0x%08X).\n",
                magic);
        goto fail;
    }

    // 校验版本号
    if (fread(&version, sizeof(int), 1, fp) != 1) {
        fprintf(stderr, "ERROR: Failed to read order file version.\n");
        goto fail;
    }
    if (version != FILE_VERSION) {
        fprintf(stderr, "ERROR: Unsupported order file version %d (expected %d).\n",
                version, FILE_VERSION);
        goto fail;
    }

    // 读取订单总数
    if (fread(&count, sizeof(int), 1, fp) != 1) {
        fprintf(stderr, "ERROR: Failed to read order count.\n");
        goto fail;
    }

    max_capacity = (int)(sizeof(queue->items) / sizeof(queue->items[0]));
    if (count < 0 || count > max_capacity) {
        fprintf(stderr, "ERROR: Order count %d out of range [0, %d].\n",
                count, max_capacity);
        goto fail;
    }

    // 队列为空直接返回
    if (count == 0) {
        queue_init(queue);
        fclose(fp);
        return 0;
    }

    // 按保存时的顺序逐个读取（保存时已将循环队列展平为线性序列）
    for (int i = 0; i < count; i++) {
        if (fread(&queue->items[i], sizeof(StockOrder), 1, fp) != 1) {
            fprintf(stderr, "ERROR: Failed to read order data at index %d.\n", i);
            goto fail;
        }
    }

    // 🔧 修复：还原队列内部状态
    // 保存时从 front 开始线性写入，加载时数据从 items[0] 连续排列，
    // 因此 front=0，rear=count%capacity（修正 count==capacity 时的溢出）
    queue->size = count;
    queue->front = 0;
    queue->rear = count % max_capacity;  // 🔧 修复：count==256 时 rear=0 而非 256

    fclose(fp);
    return 0;

fail:
    if (fp != NULL) {
        fclose(fp);
    }
    return -1;
}