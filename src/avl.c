#include "all.h"

// ------------------- 内部辅助静态函数 -------------------
static int get_height(AVLNode* node)
{
    return node ? node->height : 0;
}

static void update_height(AVLNode* node)
{
    if (!node) return;
    int lh = get_height(node->left);
    int rh = get_height(node->right);
    node->height = (lh > rh ? lh : rh) + 1;
}

static int get_balance(AVLNode* node)
{
    return node ? get_height(node->left) - get_height(node->right) : 0;
}

// 右旋转 LL
static AVLNode* right_rotate(AVLNode* y)
{
    AVLNode* x = y->left;
    AVLNode* t2 = x->right;

    x->right = y;
    y->left = t2;

    update_height(y);
    update_height(x);
    return x;
}

// 左旋转 RR
static AVLNode* left_rotate(AVLNode* x)
{
    AVLNode* y = x->right;
    AVLNode* t2 = y->left;

    y->left = x;
    x->right = t2;

    update_height(x);
    update_height(y);
    return y;
}

// 获取右子树最小节点（删除用，中序后继）
static AVLNode* avl_get_min_node(AVLNode* root)
{
    AVLNode* cur = root;
    while (cur->left != NULL)
        cur = cur->left;
    return cur;
}

// -------------------对外接口实现，与avl.h一一对应-------------------

AVLNode* avl_create_node(Product *p, const char *sku)
{
    if (!p|| !sku) return NULL;
    AVLNode* node = (AVLNode*)malloc(sizeof(AVLNode));
    if (!node) return NULL;

    strncpy(node->sku, sku, sizeof(node->sku)-1);
    node->sku[sizeof(node->sku)-1] = '\0';

    node->product = p;
    node->height = 1;
    node->left = NULL;
    node->right = NULL;
    return node;
}

AVLNode* avl_insert(AVLNode *root, AVLNode *node)
{
    if (root == NULL)
        return node;

    // sku比较
    int cmp = strcmp(node->sku, root->sku);
    if (cmp < 0)
    {
        root->left = avl_insert(root->left, node);
    }
    else if (cmp > 0)
    {
        root->right = avl_insert(root->right, node);
    }
    else
    {
        // sku重复，直接返回原root，不插入重复key
        return root;
    }

    update_height(root);
    int bal = get_balance(root);

    // LL
    if (bal > 1 && strcmp(node->sku, root->left->sku) < 0)
        return right_rotate(root);

    // RR
    if (bal < -1 && strcmp(node->sku, root->right->sku) > 0)
        return left_rotate(root);

    // LR
    if (bal > 1 && strcmp(node->sku, root->left->sku) > 0)
    {
        root->left = left_rotate(root->left);
        return right_rotate(root);
    }

    // RL
    if (bal < -1 && strcmp(node->sku, root->right->sku) < 0)
    {
        root->right = right_rotate(root->right);
        return left_rotate(root);
    }

    return root;
}

AVLNode* avl_delete(AVLNode *root, const char *sku)
{
    if (!root) return NULL;

    int cmp = strcmp(sku, root->sku);
    if (cmp < 0)
    {
        root->left = avl_delete(root->left, sku);
    }
    else if (cmp > 0)
    {
        root->right = avl_delete(root->right, sku);
    }
    else
    {
        // 找到待删除节点
        AVLNode* temp;
        if (root->left == NULL || root->right == NULL)
        {
            temp = root->left ? root->left : root->right;
            // 0或1个子节点
            if (temp == NULL)
            {
                temp = root;
                root = NULL;
            }
            else
            {
                *root = *temp; // 拷贝内容，接管子树
            }
            free(temp);
        }
        else
        {
            // 两个子节点：取后继（右子树最小）
            AVLNode* succ = avl_get_min_node(root->right);
            strncpy(root->sku, succ->sku, sizeof(root->sku)-1);
            root->sku[sizeof(root->sku)-1] = '\0';
            root->product = succ->product;

            root->right = avl_delete(root->right, succ->sku);
        }
    }

    if (root == NULL)
        return NULL;

    update_height(root);
    int bal = get_balance(root);

    // LL
    if (bal > 1 && get_balance(root->left) >= 0)
        return right_rotate(root);
    // LR
    if (bal > 1 && get_balance(root->left) < 0)
    {
        root->left = left_rotate(root->left);
        return right_rotate(root);
    }
    // RR
    if (bal < -1 && get_balance(root->right) <= 0)
        return left_rotate(root);
    // RL
    if (bal < -1 && get_balance(root->right) > 0)
    {
        root->right = right_rotate(root->right);
        return left_rotate(root);
    }

    return root;
}

AVLNode* sched_search(AVLNode *root, const char *sku,int flag)
{
    if (!root) return NULL;
	if(flag != 0)
	{
        printf("正在访问节点 sku=%s\n", root->sku);
    }
    int cmp = strcmp(sku, root->sku);
    if (cmp == 0)
        return root;
    else if (cmp < 0)
        return sched_search(root->left, sku,flag);
    else
        return sched_search(root->right, sku,flag);
}

void avl_destroy(AVLNode *root)
{
    if (!root) return;
    avl_destroy(root->left);
    avl_destroy(root->right);
    // !!!只释放AVL节点，Product由双向循环链表负责释放，不要free(root->product)!!!
    free(root);
}
