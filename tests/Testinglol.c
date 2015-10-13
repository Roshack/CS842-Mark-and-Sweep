#include <stdio.h>
#include <stdlib.h>

#include "ggggc/gc.h"


GGC_TYPE(treeNode)
    GGC_MPTR(treeNode, left);
    GGC_MPTR(treeNode, right);
    GGC_MDATA(long, item);
GGC_END_TYPE(treeNode,
    GGC_PTR(treeNode, left)
    GGC_PTR(treeNode, right)
    )


treeNode NewTreeNode(treeNode left, treeNode right, long item)
{
    treeNode    newT = NULL;

    GGC_PUSH_3(left, right, newT);

    newT = GGC_NEW(treeNode);

    GGC_WP(newT, left, left);
    GGC_WP(newT, right, right);
    GGC_WD(newT, item, item);

    return newT;
}/* NewTreeNode() */

treeNode TopDownTree(long item, unsigned depth)
{
    if (depth > 0) {
        treeNode ret, l, r;
        ret = l = r = NULL;
        GGC_PUSH_3(ret, l, r);

        ret = NewTreeNode(NULL, NULL, item);
        l = TopDownTree(2 * item - 1, depth - 1);
        r = TopDownTree(2 * item, depth - 1);
        GGC_WP(ret, left, l);
        GGC_WP(ret, right, r);

        return ret;
    } else
        return NewTreeNode(NULL, NULL, item);
} /* BottomUpTree() */

void DestroyMyInsides()
{
    treeNode x = NewTreeNode(NULL,NULL,2);
    GGC_PUSH_1(x);
    printf("Hey I allocated x at %lx\r\n", (long unsigned int) ((void *) x));
    return;

}

int main(int argc, char* argv[])
{
    DestroyMyInsides();
    treeNode x = NULL;
    treeNode y = NULL;
    treeNode z = NULL;
    treeNode first = NULL;
    treeNode prev = NULL;
    GGC_PUSH_5(x, y, z,first,prev);
    x = NewTreeNode(y,z,5);
    DestroyMyInsides();
    y = NewTreeNode(x,z,12);
    GGC_WP(x,left,y);
    GGC_WP(y,left,z);
    x = NewTreeNode(y,y,5);
    GGC_WP(y,right,x);
    GGC_WP(y,left,x);
    y = NULL;
    y = NewTreeNode(x,x,2);
    z = NewTreeNode(x,y,5);
    DestroyMyInsides();
    ggggc_collect();
    z = NewTreeNode(x,y,5);
    z = NewTreeNode(x,y,5);
    z = NewTreeNode(x,y,5);
    z = NewTreeNode(x,y,5);
    z = NewTreeNode(x,y,5);
    z = NewTreeNode(x,y,5);
    z = NewTreeNode(x,y,5);
    z = NewTreeNode(x,y,5);

    return 0;

}
