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
    newT = GGC_NEW(treeNode);

    return newT;
} /* NewTreeNode() */
int main(int argc, char* argv[])
{
    treeNode *x;
    printf("Tried to make a treenode lol");
    return 0;

}
