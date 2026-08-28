/**
 * @file product.c
 * @brief 商品管理模块实现（增删改查 + AVL 索引维护）
 * 
 * 本模块实现商品数据的链表管理和 AVL 树索引：
 * - 使用有头双向循环链表存储商品数据（支持遍历）
 * - 使用 AVL 树按 SKU 建立索引（支持 O(log n) 快速查找）
 * - 提供增、删、改、查、计数、遍历等对外接口
 * 
 * 依赖模块：avl.h（AVL 树操作）
 */
#include "all.h"  /* 替换原 #include "all.h"（原本指向不存在的文件，现在 all.h 已实现） */

/* ---------- 商品节点创建 ---------- */
/**
 * @brief 创建一个商品节点（分配内存并初始化）
 *
 * @param sku   商品编号（唯一标识，长度不超过 31 字符）
 * @param name  商品名称（长度不超过 63 字符）
 * @param stock 初始库存数量
 * @return Product* 成功返回新节点指针，失败返回 NULL
 *
 * @note 该函数只负责创建节点，不负责将节点加入链表。
 *       调用者需自行调用 list_add() 加入容器。
 */
Product* product_create(const char *sku, const char *name, int stock)
{
    Product *p = (Product*)malloc(sizeof(Product));//在堆上为 Product 结构体分配内存，返回指针 p
    if (p == NULL)  //判断 malloc 是否分配失败
    {
        return NULL;//分配失败，返回 NULL 
    }
    strcpy(p->data.sku, sku);      //将传入的 sku 字符串复制到商品数据的 sku 字段中
    strcpy(p->data.name, name);    //将传入的 name 字符串复制到商品数据的 name 字段中
    p->data.stock = stock;         //设置商品库存数量为传入的 stock 值
    p->prev = p->next = NULL;      //设置商品库存数量为传入的 stock 值
    return p;                      //返回创建好的商品节点指针给调用者
}

/* ---------- 链表操作 ---------- */
/**
 * @brief 创建商品容器（初始化空链表和 AVL 树）
 *
 * 步骤：
 *   1. 分配 ProductList 容器内存
 *   2. 分配哨兵节点（作为链表锚点）
 *   3. 哨兵节点的 prev/next 都指向自身，形成空循环链表
 *   4. AVL 树根置 NULL，计数归零
 *
 * @return ProductList* 成功返回容器指针，失败返回 NULL
 */
ProductList* list_create(void)
{
    ProductList *list = (ProductList*)malloc(sizeof(ProductList));//为 ProductList 容器结构体分配内存
    if (list == NULL)//判断容器内存分配是否失败
    {
        return NULL;//分配失败，返回 NULL 
    }

    list->head = (Product*)malloc(sizeof(Product));//在堆上为哨兵节点分配内存,不存商品数据
    if (list->head == NULL)//判断哨兵节点内存分配是否失败
    {
        free(list);//释放之前分配的容器内存
        return NULL;//返回 NULL 表示容器创建失败 
    }
    // 哨兵节点：prev 和 next 都指向自己，形成空链表（循环）
    list->head->prev = list->head;
    list->head->next = list->head;

    list->avl_root = NULL;        //初始化AVL树根为空，表示还没有创建索引
    list->count = 0;              //商品数为0，容器里面没有商品
    return list;                  //返回创建好的容器指针给调用者 
}

/*销毁容器操作*/
/**
 * @brief 销毁整个商品容器（释放所有内存）
 *
 * 释放顺序：
 *   1. 遍历链表释放所有商品节点（Product）
 *   2. 释放哨兵节点
 *   3. 释放 AVL 树节点（avl_destroy 只释放 AVL 节点，不释放 Product）
 *   4. 释放容器本身
 *
 * @param list 要销毁的容器指针（若为 NULL 则无操作）
 */
void list_destroy(ProductList *list)
{
    if (list == NULL)//判断传入的指针是否为空
    {
        return;      //指针为空直接返回
    }
    /* 释放所有商品节点*/
    Product *cur = list->head->next;   //cur 指向哨兵的下一个节点，即第一个有效商品节点
    while (cur != list->head)          //循环遍历，直到回到哨兵节点，遍历完所有商品
    {
        Product *tmp = cur;            //tmp 暂存当前节点指针，以便稍后释放
        cur = cur->next;               //cur 移动到下一个节点，在释放当前节点之前保存好位置
        free(tmp);                     //释放当前节点（tmp 指向的商品节点）内存
    }
    free(list->head);                  // 释放哨兵
    avl_destroy(list->avl_root);       // 销毁 AVL 树（仅释放节点，不释放 Product）
    free(list);                        //释放容器ProductList结构体
}
/* ================================================================
 *                    核心增删改查（CRUD）
 * ================================================================ */
/**
 * @brief 添加商品（尾插到链表 + 插入 AVL 索引）
 * 
 * 操作流程：
 *   1. 通过 AVL 查找 SKU，若已存在则拒绝添加
 *   2. 将节点尾插到双向循环链表
 *   3. 创建 AVL 节点并插入树中
 *   4. 若 AVL 插入失败，回滚链表插入操作
 * 
 * @param list 容器指针
 * @param node 要添加的商品节点（必须由 product_create 创建）
 * @return 0 成功，-1 失败（SKU 已存在或内存分配失败）
 */
int list_add(ProductList *list, Product *node)
{
    if (list == NULL || node == NULL)//容器和节点不能为空
    {
        return -1;                   //参数无效
    }
    // 检查 SKU 是否已存在（通过 AVL 查找）
    if (sched_search(list->avl_root, node->data.sku , 0) != NULL)//调用 sched_search 在 AVL 树中查找当前 SKU 
    {
        return -1;   // 已存在
    }

    // 尾插到链表
    Product *head = list->head;//head 指向哨兵节点（链表的固定锚点）
    Product *tail = head->prev;//tail 指向哨兵的前驱节点，即当前链表的最后一个有效节点
    tail->next = node;         //将原尾节点的 next 指针指向新节点
    node->prev = tail;         //新节点的 prev 指针指向原尾节点
    node->next = head;         //新节点的 next 指针指向哨兵节点
    head->prev = node;         //哨兵的 prev 指针指向新节点

    // 创建 AVL 节点并插入树
    AVLNode *avl_node = avl_create_node(node, node->data.sku);//调用 avl_create_node 创建 AVL 节点
    if (avl_node == NULL)//判断 AVL 节点是否创建失败
    {
        // 回滚链表插入
        node->prev->next = node->next;//将前一个节点的 next 指针指向后一个节点
        node->next->prev = node->prev;//将后一个节点的 prev 指针指向前一个节点
        return -1;                    //AVL创建失败返回-1
    }
   /*调用 avl_insert 将 AVL 节点插入树中 
     返回新的根节点，并保存到容器的 avl_root 字段 
     avl_insert 内部会自动进行平衡旋转，保证树的高度平衡*/
    list->avl_root = avl_insert(list->avl_root, avl_node);
    list->count++;//商品计数器加 1，表示容器中多了一个商品
    return 0;
}

/*删除商品：根据 ID（SKU）删除，库存必须为 0*/
/**
 * @brief 删除商品（根据 ID/SKU）
 *
 * 业务约束（NT-1）：
 *   - 商品库存必须为 0 才能删除，否则返回 -2
 *
 * 操作流程：
 *   1. 通过 list_find 查找商品（内部使用 AVL）
 *   2. 检查库存是否为 0
 *   3. 从链表中解绑节点
 *   4. 从 AVL 树中删除对应索引
 *   5. 释放商品节点内存，计数减 1
 *
 * @param list 容器指针
 * @param id   要删除的商品编号（SKU）
 * @return 0 成功，-1 商品不存在，-2 库存不为 0 拒绝删除
 */
int list_remove(ProductList *list, char *id)
{
    if (list == NULL || id == NULL)//检查参数是否有效
    {
        return -1;                //参数无效
    }

    // 先查找商品
    Product *p = list_find(list, id);//调用 list_find 通过 AVL 树快速查找商品
    if (p == NULL)//判断是否找到商品
    {
        return -1;   // 不存在
    }

    if (p->data.stock != 0)//检查商品库存是否为 0（NT-1：库存为 0 才允许删除）
    {
        return -2;   // 库存不为 0，拒绝删除
    }
    // 从链表中移除
    p->prev->next = p->next;//将前一个节点的 next 指针指向后一个节点
    p->next->prev = p->prev;//将后一个节点的 prev 指针指向前一个节点

    // 从 AVL 中删除
    //调用 avl_delete 从 AVL 树中删除该 SKU 对应的节点 
    //返回新的根节点，并保存到容器的 avl_root 字段
    //avl_delete 内部会自动进行平衡旋转
    list->avl_root = avl_delete(list->avl_root, id);
    free(p);//释放商品节点 p 占用的内存
    list->count--;//商品计数器减 1，表示容器中少了一个商品
    return 0;
}

/*修改商品名称：根据 ID（SKU）修改*/
/**
 * @brief 修改商品名称（根据 ID/SKU）
 *
 * 由于 SKU 不变，AVL 树结构无需更新，只需修改链表中的名称字段。
 *
 * @param list     容器指针
 * @param id       要修改的商品编号（SKU）
 * @param new_name 新的商品名称
 * @return 0 成功，-1 商品不存在
 */
int list_update(ProductList *list, char *id, const char *new_name)
{
    Product *p = list_find(list, id);//调用 list_find 通过 AVL 树查找商品
    if (p == NULL)// 判断是否找到商品
    {
        return -1;
    }
    strcpy(p->data.name, new_name);//直接修改商品数据中的名称字段为新名称
    return 0;
}

/*查找商品：根据 ID（SKU）查找*/
/**
 * @brief 查找商品（根据 ID/SKU）
 *
 * 使用 AVL 树实现 O(log n) 快速查找。
 *
 * @param list 容器指针
 * @param id   要查找的商品编号（SKU）
 * @return Product* 找到返回商品节点指针，未找到返回 NULL
 *
 * @note 返回的指针指向链表中的真实数据，调用者可通过该指针
 *       直接修改库存（如 p->data.stock += 10）。
 */
Product* list_find(ProductList *list, char *id)
{
    if (list == NULL || id == NULL)//检查参数有效性
        return NULL;//无效返回NULL
    /* 调用 sched_search 在 AVL 树中查找该 SKU */
    /* 返回 AVL 节点指针（包含 sku、product 指针、height、left、right） */
    AVLNode *n = sched_search(list->avl_root, id, 0);
    return n ? n->product : NULL;//如果 n 不为 NULL，返回 n->product 如果 n 为 NULL，返回NULL
}

/*获取商品总数*/
/**
 * @brief 获取当前商品总数
 *
 * @param list 容器指针
 * @return 商品数量（若 list 为 NULL 则返回 0）
 */
int list_get_count(ProductList *list)
{
    return list ? list->count : 0;// 三元运算符：如果 list 不为 NULL，返回 count 字段，为NULL返回0
}

/**
 * @brief 获取第一个有效商品节点（用于遍历）
 * 
 * 遍历示例：
 *   Product *cur = list_get_head(list);
 *   while (cur != NULL) {
 *       // 处理 cur->data ...
 *       cur = cur->next;
 *       if (cur == list->head) break;
 *   }
 * 
 * @param list 容器指针
 * @return Product* 第一个商品节点，若链表为空或 list 为 NULL 则返回 NULL
 * 
 * @note 返回的是哨兵节点的下一个节点（即第一个有效数据节点），
 *       而非哨兵节点本身。
 */
Product* list_get_head(ProductList *list)
{
    if (list == NULL || list->head->next == list->head)//判断是否为有效节点
    {
        return NULL;
    }
    return list->head->next;//返回哨兵的下一个节点，即链表的第一个有效商品节点
}
