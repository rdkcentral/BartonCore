
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "linkedTest.h"
#include <icLog/logging.h>
#include <icTypes/icLinkedList.h>
#include <icUtil/stringUtils.h>

#define LOG_CAT "logTEST"

typedef struct _sample
{
    int num;
    char label[50];
} sample;

static sample *createSample()
{
    sample *retVal = (sample *) malloc(sizeof(sample));
    memset(retVal, 0, sizeof(sample));
    return retVal;
}

static void printSample(sample *curr)
{
    icLogDebug(LOG_CAT, " sample num=%d label=%s", curr->num, curr->label);
}

static bool printIterator(void *item, void *arg)
{
    // typecast to 'sample'
    sample *curr = (sample *) item;
    printSample(curr);
    return true;
}

static void printList(icLinkedList *list)
{
    // loop through nodes of list, calling 'printIterator'
    //
    linkedListIterate(list, printIterator, NULL);
}

static icLinkedList *makePopulatedList(int entryCount)
{
    icLinkedList *list = linkedListCreate();

    int i = 0;
    sample *item = NULL;
    for (i = 0; i < entryCount; i++)
    {
        item = createSample();
        item->num = (i + 1);
        sprintf(item->label, "testing %d", item->num);
        linkedListAppend(list, item);
    }
    return list;
}

static char *getLabelCopyFromSample(sample *sample)
{
    return strdup(sample->label);
}

static void *collectLabels(void *item, void *context)
{
    (void) context;
    return getLabelCopyFromSample(item);
}

static MapFuncKeyContext *mapLabels(void *item, void *context)
{
    bool *cloneSample = (bool *) context;

    MapFuncKeyContext *mapKeyCtx = calloc(1, sizeof(MapFuncKeyContext));
    mapKeyCtx->key = getLabelCopyFromSample(item);
    mapKeyCtx->keyLen = (mapKeyCtx->key != NULL) ? strlen(mapKeyCtx->key) : 0;
    if (*cloneSample == true)
    {
        mapKeyCtx->value = calloc(1, sizeof(sample));
        memcpy(mapKeyCtx->value, item, sizeof(sample));
    }
    else
    {
        mapKeyCtx->value = item;
    }

    return mapKeyCtx;
}

static void freeLabelFromMap(void *key, void *value)
{
    // Don't free sample, it belongs to a list originally
    (void) value;
    free(key);
}

static void freeLabelAndSampleFromMap(void *key, void *value)
{
    free(key);
    free(value);
}

static void *foldSamplesSum(void *sum, void *item, void *context)
{
    (void) context;

    sample *sample1 = (sample *) item;
    size_t *total = (size_t *) sum;
    *total += sample1->num;

    return total;
}

/*
 * test creating a linked list (0 itmes)
 */
bool canCreate()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    // create then destroy a icLinkedList
    //
    icLinkedList *list = linkedListCreate();
    linkedListDestroy(list, NULL);

    return true;
}

/*
 * test adding items to the end of the linked list
 */
bool canAppend()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    icLinkedList *list = linkedListCreate();
    int count = 0;
    int i = 0;
    sample *item = NULL;

    // make initial entry
    //
    item = createSample();
    item->num = 0;
    strcpy(item->label, "testing 1,2,3");
    linkedListAppend(list, item);

    // check size
    //
    count = linkedListCount(list);
    icLogDebug(LOG_CAT, "  %s - appended 1 entry, count = %d", __FUNCTION__, count);
    if (count != 1)
    {
        linkedListDestroy(list, NULL);
        return false;
    }

    // add 4 more
    //
    for (i = 0; i < 4; i++)
    {
        item = createSample();
        item->num = (i + 1);
        strcpy(item->label, "testing 1,2,3");
        linkedListAppend(list, item);
    }

    // check size
    //
    count = linkedListCount(list);
    icLogDebug(LOG_CAT, "  %s - appended 5 entries, count = %d", __FUNCTION__, count);
    if (count != 5)
    {
        linkedListDestroy(list, NULL);
        return false;
    }

    printList(list);
    linkedListDestroy(list, NULL);

    return true;
}

/*
 * prepend items to make sure they are inserted at the front properly
 */
bool canPrepend()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    // test creating a linked list (0 itmes)
    //
    icLinkedList *list = linkedListCreate();

    int count = 0;
    int i = 0;
    sample *item = NULL;

    // make initial entry
    //
    item = createSample();
    item->num = 0;
    strcpy(item->label, "testing 1,2,3");
    linkedListPrepend(list, item);

    // check size
    //
    count = linkedListCount(list);
    icLogDebug(LOG_CAT, "  %s - prepended 1 entry, count = %d", __FUNCTION__, count);
    if (count != 1)
    {
        linkedListDestroy(list, NULL);
        return false;
    }

    // add 4 more
    //
    for (i = 0; i < 4; i++)
    {
        item = createSample();
        item->num = (i + 1);
        strcpy(item->label, "testing 1,2,3");
        linkedListPrepend(list, item);
    }

    // check size
    //
    count = linkedListCount(list);
    icLogDebug(LOG_CAT, "  %s - prepended 5 entries, count = %d", __FUNCTION__, count);
    if (count != 5)
    {
        linkedListDestroy(list, NULL);
        return false;
    }

    printList(list);
    linkedListDestroy(list, NULL);

    return true;
}

bool sampleSearch(void *searchVal, void *item)
{
    // typecast to 'sample'
    sample *curr = (sample *) item;
    int search = *((int *) searchVal);

    // looking for matching 'num'
    if (search == curr->num)
    {
        return true;
    }

    return false;
}

/*
 * create list of 5 items, then find some of them
 */
bool canFind()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    // make list of 5 items
    //
    int i = 0;
    icLinkedList *list = makePopulatedList(5);
    printList(list);

    // try to find the one with "num=2"
    //
    i = 2;
    void *found = linkedListFind(list, &i, sampleSearch);
    if (found == NULL)
    {
        icLogWarn(LOG_CAT, "unable to locate sample #2");
        linkedListDestroy(list, NULL);
        return false;
    }
    printSample((sample *) found);

    // try to find the one with "num=2"
    //
    i = 5;
    found = linkedListFind(list, &i, sampleSearch);
    if (found == NULL)
    {
        icLogWarn(LOG_CAT, "unable to locate sample #5");
        linkedListDestroy(list, NULL);
        return false;
    }
    printSample((sample *) found);

    linkedListDestroy(list, NULL);

    return true;
}

/*
 * Test the simple linkedListStringCompareSearchFunc
 */
bool canFindBasicString(void)
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    // make list of 5 items
    //
    int i = 0;
    icLinkedList *list = linkedListCreate();
    for (i = 0; i < 5; i++)
    {
        char *text = malloc(6);
        sprintf(text, "test%d", i);
        linkedListAppend(list, text);
    }

    // try to find the one with "test3"
    //
    void *found = linkedListFind(list, "test3", linkedListStringCompareSearchFunc);
    if (found == NULL)
    {
        icLogWarn(LOG_CAT, "unable to locate test3");
        linkedListDestroy(list, NULL);
        return false;
    }

    linkedListDestroy(list, NULL);

    return true;
}

/*
 *
 */
bool canNotFind()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    // make list of 5 items
    //
    int i = 0;
    icLinkedList *list = makePopulatedList(5);

    // call find, but something not there
    //
    i = 102;
    void *found = linkedListFind(list, &i, sampleSearch);
    if (found != NULL)
    {
        icLogWarn(LOG_CAT, "found sample that we should not have");
        linkedListDestroy(list, NULL);
        return false;
    }

    linkedListDestroy(list, NULL);
    return true;
}

/*
 *
 */
bool canDelete()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    // make list of 5 items
    //
    int i = 0;
    icLinkedList *list = makePopulatedList(5);

    // delete #4
    i = 4;
    if (linkedListDelete(list, &i, sampleSearch, NULL) != true)
    {
        icLogWarn(LOG_CAT, "unable to delete node #4");
        linkedListDestroy(list, NULL);
        return false;
    }
    printList(list);

    // delete #0
    i = 1;
    if (linkedListDelete(list, &i, sampleSearch, NULL) != true)
    {
        icLogWarn(LOG_CAT, "unable to delete node #1");
        linkedListDestroy(list, NULL);
        return false;
    }
    printList(list);

    // delete #5
    i = 5;
    if (linkedListDelete(list, &i, sampleSearch, NULL) != true)
    {
        icLogWarn(LOG_CAT, "unable to delete node #5");
        linkedListDestroy(list, NULL);
        return false;
    }
    printList(list);

    linkedListDestroy(list, NULL);
    return true;
}

/*
 * test alternative iterator
 */
bool canIterateAlternative()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    // make list of 5 items
    //
    int i = 0;
    icLinkedList *list = makePopulatedList(5);
    //    printList(list);

    // get an iterator
    //
    icLinkedListIterator *loop = linkedListIteratorCreate(list);
    while (linkedListIteratorHasNext(loop) == true)
    {
        void *item = linkedListIteratorGetNext(loop);
        sample *curr = (sample *) item;
        printSample(curr);
        i++;
    }

    sample *nonExistent = linkedListIteratorGetNext(loop);
    if (nonExistent != NULL)
    {
        icLogError(LOG_CAT, "Iterator returns items after traversal");
        return false;
    }

    // done with loop, see if we got the correct number
    //
    if (i != 5)
    {
        icLogWarn(LOG_CAT, "unable to iterate all nodes properly, got %d expected 5", i);
        linkedListIteratorDestroy(loop);
        linkedListDestroy(list, NULL);
        return false;
    }

    linkedListIteratorDestroy(loop);
    linkedListDestroy(list, NULL);

    return true;
}


/*
 * test deleting an item from within the iterator
 */
bool canDeleteFromIterator()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    // make list of 5 items
    //
    int i = 0;
    icLinkedList *list = makePopulatedList(5);

    // get an iterator
    //
    icLinkedListIterator *loop = linkedListIteratorCreate(list);

    // make sure we cannot delete before the first call to get next
    //
    if (linkedListIteratorDeleteCurrent(loop, NULL) == true)
    {
        icLogWarn(LOG_CAT, "able to delete prematurely from iterator!");
        linkedListIteratorDestroy(loop);
        linkedListDestroy(list, NULL);
        return false;
    }

    while (linkedListIteratorHasNext(loop) == true)
    {
        void *item = linkedListIteratorGetNext(loop);
        sample *curr = (sample *) item;
        printSample(curr);
        i++;

        // try first, middle & end
        if (i == 1 || i == 3 || i == 5)
        {
            // delete this item
            icLogDebug(LOG_CAT, "deleting item #%d", i);
            linkedListIteratorDeleteCurrent(loop, NULL);
        }
    }
    linkedListIteratorDestroy(loop);
    printList(list);

    // done with loop, see if we correcly removed 2 items
    //
    i = linkedListCount(list);
    if (i != 2)
    {
        icLogWarn(LOG_CAT, "unable to delete from iterator, got %d expected 2", i);
        linkedListDestroy(list, NULL);
        return false;
    }

    linkedListDestroy(list, NULL);
    return true;
}

static void *cloneItem(void *item, void *context)
{
    if (strcmp(context, "context") != 0)
    {
        icLogError(LOG_CAT, "Unexpected context object");
        return NULL;
    }

    sample *orig = (sample *) item;
    sample *copy = createSample();
    copy->num = orig->num;
    strcpy(copy->label, orig->label);

    return copy;
}

bool canDeepClone()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    // make list of 5 items
    //
    char context[50] = "context";
    icLinkedList *list = makePopulatedList(5);
    icLinkedList *copy = linkedListDeepClone(list, cloneItem, context);
    if (linkedListCount(copy) != 5)
    {
        icLogError(LOG_CAT, "Deep cloned list does not contain all items");
        linkedListDestroy(list, NULL);
        linkedListDestroy(copy, NULL);
        return false;
    }

    bool retVal = true;
    icLinkedListIterator *origIter = linkedListIteratorCreate(list);
    icLinkedListIterator *copyIter = linkedListIteratorCreate(copy);
    while (linkedListIteratorHasNext(origIter) && linkedListIteratorHasNext(copyIter))
    {
        sample *origItem = linkedListIteratorGetNext(origIter);
        sample *copyItem = linkedListIteratorGetNext(copyIter);

        if (origItem->num != copyItem->num)
        {
            icLogError(LOG_CAT, "Copied item num does not match");
            retVal = false;
            break;
        }

        if (strcmp(origItem->label, copyItem->label) != 0)
        {
            icLogError(LOG_CAT, "Copied item label does not match");
            retVal = false;
            break;
        }
    }
    linkedListIteratorDestroy(origIter);
    linkedListIteratorDestroy(copyIter);
    linkedListDestroy(list, NULL);
    if (retVal == false)
    {
        linkedListDestroy(copy, NULL);
        return retVal;
    }

    // Test can still use after deleting of original list
    copyIter = linkedListIteratorCreate(copy);
    while (linkedListIteratorHasNext(copyIter))
    {
        sample *copyItem = linkedListIteratorGetNext(copyIter);
        icLogDebug(LOG_CAT, "at item #%d with label %s", copyItem->num, copyItem->label);
    }
    linkedListIteratorDestroy(copyIter);
    linkedListDestroy(copy, NULL);

    return true;
}

bool canShallowCloneList()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    icLinkedList *list = makePopulatedList(5);
    icLinkedList *clonedList = linkedListClone(list);

    if (linkedListIsClone(clonedList) == false)
    {
        icLogError(LOG_CAT, "Shallow cloned list is not marked as cloned");
        linkedListDestroy(list, NULL);
        linkedListDestroy(clonedList, NULL);
        return false;
    }

    if (linkedListCount(clonedList) != 5)
    {
        icLogError(LOG_CAT, "Shallow cloned list does not contain all items");
        linkedListDestroy(list, NULL);
        linkedListDestroy(clonedList, NULL);
        return false;
    }

    icLinkedListIterator *origIter = linkedListIteratorCreate(list);
    icLinkedListIterator *copyIter = linkedListIteratorCreate(clonedList);

    bool retVal = true;
    while (linkedListIteratorHasNext(origIter) && linkedListIteratorHasNext(copyIter))
    {
        sample *origItem = linkedListIteratorGetNext(origIter);
        sample *copyItem = linkedListIteratorGetNext(copyIter);

        if (origItem->num != copyItem->num)
        {
            icLogError(LOG_CAT, "Copied item num does not match");
            retVal = false;
            break;
        }

        if (origItem->label != copyItem->label)
        {
            icLogError(LOG_CAT, "Copied item label does not match");
            retVal = false;
            break;
        }
    }
    linkedListIteratorDestroy(origIter);
    linkedListIteratorDestroy(copyIter);

    linkedListDestroy(list, NULL);
    linkedListDestroy(clonedList, NULL);

    return retVal;
}

bool canGetElementAt()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    icLinkedList *list = makePopulatedList(5);
    uint32_t offset = 6;

    sample *retVal = linkedListGetElementAt(list, offset);
    if (retVal != NULL)
    {
        icLogError(LOG_CAT, "Got element from index outside of list size");
        linkedListDestroy(list, NULL);
        return false;
    }

    offset = 3;
    retVal = linkedListGetElementAt(list, offset);
    if (retVal == NULL)
    {
        icLogError(LOG_CAT, "Failed to remove valid element");
        linkedListDestroy(list, NULL);
        return false;
    }

    icLinkedListIterator *listIter = linkedListIteratorCreate(list);
    size_t i = 0;
    bool isCorrect = false;
    while (linkedListIteratorHasNext(listIter))
    {
        sample *element = linkedListIteratorGetNext(listIter);
        if (i == 3 && element == retVal)
        {
            isCorrect = true;
        }
        i++;
    }
    linkedListIteratorDestroy(listIter);

    bool rc = true;
    if (isCorrect == false)
    {
        icLogError(LOG_CAT, "Did not get the correct element");
        rc = false;
    }
    linkedListDestroy(list, NULL);

    return rc;
}

bool canRemovElementAt()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    icLinkedList *list = makePopulatedList(5);
    uint32_t offset = 6;

    sample *retVal = linkedListRemove(list, offset);
    if (retVal != NULL)
    {
        icLogError(LOG_CAT, "Removed index outside of list size");
        linkedListDestroy(list, NULL);
        return false;
    }

    offset = 3;
    sample *element = linkedListGetElementAt(list, offset);
    retVal = linkedListRemove(list, offset);
    if (retVal == NULL)
    {
        icLogError(LOG_CAT, "Failed to remove valid element");
        linkedListDestroy(list, NULL);
        return false;
    }

    if (linkedListCount(list) != 4)
    {
        icLogError(LOG_CAT, "Reported size is wrong, despite removing element");
        free(retVal);
        linkedListDestroy(list, NULL);
        return false;
    }

    if (element != retVal)
    {
        icLogError(LOG_CAT, "Wrong element returned");
        free(retVal);
        linkedListDestroy(list, NULL);
        return false;
    }

    free(retVal);
    linkedListDestroy(list, NULL);

    return true;
}

// Helper function for iterate. Adds seven to each element.
bool addSevenToAll(void *item, void *arg)
{
    sample *element = (sample *) item;

    if (element == NULL)
    {
        return false;
    }

    element->num += 7;
    return true;
}

bool canIterateList()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    icLinkedList *list = makePopulatedList(5);

    int numbers[5];
    icLinkedListIterator *iter = linkedListIteratorCreate(list);
    size_t i = 0;
    while (linkedListIteratorHasNext(iter))
    {
        sample *elem = linkedListIteratorGetNext(iter);
        if (elem != NULL)
        {
            numbers[i++] = elem->num;
        }
    }

    linkedListIteratorDestroy(iter);

    linkedListIterate(list, addSevenToAll, NULL);

    iter = linkedListIteratorCreate(list);
    i = 0;
    while (linkedListIteratorHasNext(iter))
    {
        sample *elem = linkedListIteratorGetNext(iter);
        if (elem != NULL)
        {
            if (elem->num != numbers[i] + 7)
            {
                icLogError(LOG_CAT, "The iteration function didn't apply");
                return false;
            }
        }

        i++;
    }

    linkedListIteratorDestroy(iter);

    linkedListDestroy(list, NULL);

    return true;
}

bool scopeBoundIteratorIsNotLeaky()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    icLinkedList *list = makePopulatedList(5);

    for (int i = 0; i < 2; i++)
    {
        sbIcLinkedListIterator *iter = linkedListIteratorCreate(list);
        while (linkedListIteratorHasNext(iter))
        {
            void *testVal = linkedListIteratorGetNext(iter);
            if (!testVal)
            {
                puts("Test list returned NULL test value");
                return false;
            }
        }
    }

    linkedListDestroy(list, NULL);

    /* libasan will detect any leaks */
    return true;
}

bool canClearList()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    size_t size = 5;
    icLinkedList *list = makePopulatedList(size);
    // Will use later. Assume cloning is fully tested already
    icLinkedList *clonedList = linkedListClone(list);

    if (linkedListIsClone(clonedList) == false)
    {
        icLogError(LOG_CAT, "Cloned list is not marked as cloned");
        linkedListDestroy(list, NULL);
        linkedListDestroy(clonedList, NULL);
        return false;
    }

    icLinkedListIterator *iter = linkedListIteratorCreate(list);
    size_t count = 0;
    while (linkedListIteratorHasNext(iter))
    {
        linkedListIteratorGetNext(iter);
        count += 1;
    }
    linkedListIteratorDestroy(iter);

    if (size != linkedListCount(list) && count != size)
    {
        icLogError(LOG_CAT, "Sizes don't match during clear list");
        linkedListDestroy(list, NULL);
        linkedListDestroy(clonedList, NULL);
        return false;
    }

    linkedListClear(list, NULL);
    if (linkedListCount(list) != 0)
    {
        icLogError(LOG_CAT, "Cleared list still has size");
        linkedListDestroy(list, NULL);
        linkedListDestroy(clonedList, NULL);
        return false;
    }

    iter = linkedListIteratorCreate(list);
    count = 0;
    while (linkedListIteratorHasNext(iter))
    {
        linkedListIteratorGetNext(iter);
        count += 1;
    }
    linkedListIteratorDestroy(iter);

    if (count != 0)
    {
        icLogError(LOG_CAT, "Cleared list still has stuff");
        linkedListDestroy(list, NULL);
        linkedListDestroy(clonedList, NULL);
        return false;
    }

    linkedListClear(clonedList, NULL);
    if (linkedListIsClone(clonedList) != false)
    {
        icLogError(LOG_CAT, "Cloned list not updated after clear");
        linkedListDestroy(list, NULL);
        linkedListDestroy(clonedList, NULL);
        return false;
    }

    if (linkedListCount(clonedList) != 0)
    {
        icLogError(LOG_CAT, "Cleared cloned list still has size");
        linkedListDestroy(list, NULL);
        linkedListDestroy(clonedList, NULL);
        return false;
    }

    iter = linkedListIteratorCreate(clonedList);
    count = 0;
    while (linkedListIteratorHasNext(iter))
    {
        linkedListIteratorGetNext(iter);
        count += 1;
    }
    linkedListIteratorDestroy(iter);

    if (count != 0)
    {
        icLogError(LOG_CAT, "Cleared cloned list still has stuff");
        linkedListDestroy(list, NULL);
        linkedListDestroy(clonedList, NULL);
        return false;
    }

    linkedListDestroy(list, NULL);
    linkedListDestroy(clonedList, NULL);

    return true;
}

bool canReverse(void)
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    size_t size = 5;
    icLinkedList *list = makePopulatedList(size);
    icLinkedList *reverse = linkedListReverse(list);

    for (int i = 0; i < size; i++)
    {
        const sample *sample1 = linkedListGetElementAt(list, i);
        const sample *sample2 = linkedListGetElementAt(reverse, size - i - 1);
        if (memcmp(sample1, sample2, sizeof(sample)) != 0)
        {
            icLogError(LOG_CAT, "List order is not reversed properly");
            linkedListDestroy(reverse, standardDoNotFreeFunc);
            linkedListDestroy(list, NULL);
            return false;
        }
    }

    // empty check
    linkedListDestroy(reverse, standardDoNotFreeFunc);
    linkedListDestroy(list, NULL);
    list = linkedListCreate();
    reverse = linkedListReverse(list);

    if (reverse == NULL || linkedListCount(list) != 0 || linkedListCount(reverse) != 0)
    {
        icLogError(LOG_CAT, "Empty List is not reversed properly");
        linkedListDestroy(reverse, standardDoNotFreeFunc);
        linkedListDestroy(list, NULL);
        return false;
    }

    // null check
    linkedListDestroy(reverse, standardDoNotFreeFunc);
    linkedListDestroy(list, NULL);
    list = NULL;
    reverse = linkedListReverse(list);

    if (reverse != NULL)
    {
        icLogError(LOG_CAT, "NULL List is not reversed properly");
        linkedListDestroy(reverse, standardDoNotFreeFunc);
        return false;
    }

    return true;
}

bool canFilter(void)
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    size_t size = 5;
    icLinkedList *list = makePopulatedList(size);
    icLinkedList *sampleLabels = linkedListFilter(list, collectLabels, NULL);

    if (linkedListCount(sampleLabels) != linkedListCount(list))
    {
        icLogError(LOG_CAT, "Collected sample labels list is not the same size as original list");
        linkedListDestroy(sampleLabels, NULL);
        linkedListDestroy(list, NULL);
        return false;
    }

    for (int i = 0; i < size; i++)
    {
        sample *sample1 = linkedListGetElementAt(list, i);
        char *sample2 = linkedListGetElementAt(sampleLabels, i);

        if (stringCompare(sample1->label, sample2, false) != 0)
        {
            icLogError(LOG_CAT, "Collected sample labels are not the same as original list sample labels");
            linkedListDestroy(sampleLabels, NULL);
            linkedListDestroy(list, NULL);
            return false;
        }
    }

    // empty check
    linkedListDestroy(sampleLabels, NULL);
    linkedListDestroy(list, NULL);
    list = linkedListCreate();
    sampleLabels = linkedListFilter(list, collectLabels, NULL);

    if (sampleLabels == NULL || linkedListCount(list) != 0 || linkedListCount(sampleLabels) != 0)
    {
        icLogError(LOG_CAT, "Collection not properly applied to empty list");
        linkedListDestroy(sampleLabels, NULL);
        linkedListDestroy(list, NULL);
        return false;
    }

    // null check
    linkedListDestroy(sampleLabels, NULL);
    linkedListDestroy(list, NULL);
    list = NULL;
    sampleLabels = linkedListFilter(list, collectLabels, NULL);

    if (sampleLabels != NULL)
    {
        icLogError(LOG_CAT, "Collection not properly applied to NULL list");
        linkedListDestroy(sampleLabels, NULL);
        return false;
    }

    return true;
}

bool canMap(void)
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    size_t size = 5;
    icLinkedList *list = makePopulatedList(size);
    bool copySamples = false;
    icHashMap *labelToSamples = linkedListMapFromList(list, mapLabels, &copySamples, freeLabelFromMap);

    if (hashMapCount(labelToSamples) != linkedListCount(list))
    {
        icLogError(LOG_CAT, "Mapped samples is not the same size as original list");
        hashMapDestroy(labelToSamples, freeLabelFromMap);
        linkedListDestroy(list, NULL);
        return false;
    }

    for (int i = 0; i < size; i++)
    {
        sample *sample1 = linkedListGetElementAt(list, i);

        if (hashMapContains(labelToSamples, sample1->label, strlen(sample1->label)) == false)
        {
            icLogError(LOG_CAT, "Missing sample label in map");
            hashMapDestroy(labelToSamples, freeLabelFromMap);
            linkedListDestroy(list, NULL);
            return false;
        }
    }

    // With duplicate samples (mostly a memory management check)
    hashMapDestroy(labelToSamples, freeLabelFromMap);
    copySamples = true;
    labelToSamples = linkedListMapFromList(list, mapLabels, &copySamples, freeLabelAndSampleFromMap);

    if (hashMapCount(labelToSamples) != linkedListCount(list))
    {
        icLogError(LOG_CAT, "Mapped samples is not the same size as original list");
        hashMapDestroy(labelToSamples, freeLabelAndSampleFromMap);
        linkedListDestroy(list, NULL);
        return false;
    }

    for (int i = 0; i < size; i++)
    {
        sample *sample1 = linkedListGetElementAt(list, i);

        if (hashMapContains(labelToSamples, sample1->label, strlen(sample1->label)) == false)
        {
            icLogError(LOG_CAT, "Missing sample label in map");
            hashMapDestroy(labelToSamples, freeLabelAndSampleFromMap);
            linkedListDestroy(list, NULL);
            return false;
        }
    }

    // Causing issues with duplicate keys
    hashMapDestroy(labelToSamples, freeLabelAndSampleFromMap);
    sample *original = linkedListGetElementAt(list, size / 2);
    sample *duplicate = calloc(1, sizeof(sample));
    memcpy(original, duplicate, sizeof(sample));
    linkedListAppend(list, duplicate);
    size++;

    copySamples = false;
    labelToSamples = linkedListMapFromList(list, mapLabels, &copySamples, freeLabelFromMap);
    if (labelToSamples != NULL)
    {
        icLogError(LOG_CAT, "linkedListMapFromList should have returned NULL on duplicate key, but did not.");
        linkedListDestroy(list, NULL);
        return false;
    }

    // empty check
    hashMapDestroy(labelToSamples, freeLabelFromMap);
    linkedListDestroy(list, NULL);
    list = linkedListCreate();
    copySamples = false;
    labelToSamples = linkedListMapFromList(list, mapLabels, &copySamples, freeLabelFromMap);

    if (labelToSamples == NULL || linkedListCount(list) != 0 || hashMapCount(labelToSamples) != 0)
    {
        icLogError(LOG_CAT, "Map function not properly applied to empty list");
        hashMapDestroy(labelToSamples, freeLabelFromMap);
        linkedListDestroy(list, NULL);
        return false;
    }

    // null check
    hashMapDestroy(labelToSamples, freeLabelFromMap);
    linkedListDestroy(list, NULL);
    list = NULL;
    labelToSamples = linkedListMapFromList(list, mapLabels, &copySamples, freeLabelFromMap);

    if (labelToSamples != NULL)
    {
        icLogError(LOG_CAT, "Map function not properly applied to NULL list");
        hashMapDestroy(labelToSamples, freeLabelFromMap);
        return false;
    }

    return true;
}

bool canFold(void)
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    size_t size = 5;
    icLinkedList *list = makePopulatedList(size);

    // Simple accumulate test (note fold allows for far more complex building patterns such as building up a struct or
    // formatted data like json/xml)
    size_t sum1 = 0;
    scoped_icLinkedListIterator *iter = linkedListIteratorCreate(list);
    while (linkedListIteratorHasNext(iter) == true)
    {
        sample *sample1 = linkedListIteratorGetNext(iter);
        sum1 += sample1->num;
    }

    size_t sum2 = 0;
    linkedListFoldL(list, foldSamplesSum, &sum2, NULL);
    if (sum1 != sum2)
    {
        icLogError(LOG_CAT, "FoldL did not accumulate correctly: %zu != %zu", sum1, sum2);
        linkedListDestroy(list, NULL);
        return false;
    }

    sum2 = 0;
    linkedListFoldR(list, foldSamplesSum, &sum2, NULL);
    if (sum1 != sum2)
    {
        icLogError(LOG_CAT, "FoldR did not accumulate correctly: %zu != %zu", sum1, sum2);
        linkedListDestroy(list, NULL);
        return false;
    }

    // empty check
    linkedListDestroy(list, NULL);
    list = linkedListCreate();
    sum2 = 0;
    linkedListFoldL(list, foldSamplesSum, &sum2, NULL);

    if (sum2 != 0)
    {
        icLogError(LOG_CAT, "Fold function not properly applied to empty list");
        linkedListDestroy(list, NULL);
        return false;
    }

    linkedListFoldR(list, foldSamplesSum, &sum2, NULL);

    if (sum2 != 0)
    {
        icLogError(LOG_CAT, "Fold function not properly applied to empty list");
        linkedListDestroy(list, NULL);
        return false;
    }

    // null check
    linkedListDestroy(list, NULL);
    list = NULL;
    sum2 = 0;
    linkedListFoldL(list, foldSamplesSum, &sum2, NULL);

    if (sum2 != 0)
    {
        icLogError(LOG_CAT, "Fold function not properly applied to NULL list");
        linkedListDestroy(list, NULL);
        return false;
    }

    linkedListFoldR(list, foldSamplesSum, &sum2, NULL);

    if (sum2 != 0)
    {
        icLogError(LOG_CAT, "Fold function not properly applied to NULL list");
        linkedListDestroy(list, NULL);
        return false;
    }

    return true;
}

/*
 * main
 */
bool runLinkedTests()
{
    if (canCreate() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canCreate");
        return false;
    }

    if (canAppend() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canAppend");
        return false;
    }

    if (canPrepend() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canPrepend");
        return false;
    }

    if (canFind() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canFind");
        return false;
    }

    if (canFindBasicString() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canFindBasicString");
        return false;
    }

    if (canNotFind() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canNotFind");
        return false;
    }

    if (canDelete() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canDelete");
        return false;
    }

    if (canIterateAlternative() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canIterateAlternative");
        return false;
    }

    if (canDeleteFromIterator() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canDeleteFromIterator");
        return false;
    }

    if (canDeepClone() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canDeepClone");
        return false;
    }

    if (canShallowCloneList() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canShallowClone");
        return false;
    }

    if (canGetElementAt() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canGetElementAt");
        return false;
    }

    if (canRemovElementAt() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canRemoveElementAt");
        return false;
    }

    if (canIterateList() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canIterateList");
        return false;
    }

    if (canClearList() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canClearList");
        return false;
    }

    if (canReverse() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canReverse");
        return false;
    }

    if (canFilter() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canFilter");
        return false;
    }

    if (canMap() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canMap");
        return false;
    }

    if (canFold() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canFold");
        return false;
    }

    scopeBoundIteratorIsNotLeaky();

    return true;
}
