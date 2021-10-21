#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <vector>
using namespace std;

struct pageNode {
    //frame ID is physical memory frame ID, so we can keep track of where pages are going
    int frameId;
    //processPageId/virtualPageID is per process from 0 to memorySize
    int processPageId;
    // usingProcessId is the processId of the process using this page
    int usingProcessId;
    pageNode* next;
};

struct processNode {
    int memorySize;
    int serviceTime;
    int processId;
    int arrivalTime;
    int startTime;
    processNode* next;
    pageNode* pageHead;

    processNode() : arrivalTime((rand() % 60)),
                    startTime(-1),
                    next(NULL),
                    pageHead(NULL){}
};

//Global Variables
pageNode* freePageHead;
int remainingFreePages = 100;
processNode* processHead;
int timeStamp = 0;
pthread_mutex_t timerMutex= PTHREAD_MUTEX_INITIALIZER;

int getTimeStamp()
{
    int ts;
    pthread_mutex_lock(&timerMutex);
    ts = timeStamp;
    pthread_mutex_unlock(&timerMutex);
    return ts;
}

processNode* mergeSort(processNode* head)
{
    //base condition
    if(!head || !(head->next)) return head;

    //find mid of linked list
    processNode* slowPtr = head;
    processNode* fastPtr = head;
    while(fastPtr->next && fastPtr->next->next)
    {
        slowPtr = slowPtr->next;
        fastPtr = fastPtr->next->next;
    }

    //returns head pointer of sorted linked list
    processNode* head2 = mergeSort(slowPtr->next);
    slowPtr->next = NULL;
    processNode* head1 = mergeSort(head);

    processNode* tempHead = new processNode();
    head = tempHead;
    //merging two sorted linked lists
    while(head1 && head2)
    {
        if(head1->arrivalTime <= head2->arrivalTime)
        {
            head->next = head1;
            head1 = head1->next;
        }
        else
        {
            head->next = head2;
            head2 = head2->next;
        }
        head = head->next;
    }
    if(head1)
        head->next = head1;
    else if(head2)
        head->next = head2;
    head = tempHead->next;
    free(tempHead);
    return tempHead->next;
}

processNode* generateJobsAndSort( int jobs )
{
    int mem[ 4 ] = { 5, 11, 17, 31 };
    int service[ 5 ] = { 1, 2, 3, 4, 5 };

    processNode* newProcessNode;
    processNode* head = NULL;
    //create unsorted process list 
    for( int i = 0; i < jobs; i++ )
    {
        newProcessNode = new processNode();
        newProcessNode->processId = i;
        // evenly distributing memory size, service time to all the created processes
        newProcessNode->memorySize = mem[ i%4 ];
        newProcessNode->serviceTime = service[ i%5 ];
        if(!head) head = newProcessNode;
        else{
            newProcessNode->next = head;
            head = newProcessNode;
        }
    }
    return mergeSort(head);
}

pageNode* generateFreePageList( int pages )
{
    pageNode* newPageNode;
    pageNode* head = NULL;
    for ( int i = 0; i < pages; i++ )
    {
        newPageNode = new pageNode();
        newPageNode->frameId = i;
        if(!head) head = newPageNode;
        else{
            newPageNode->next = head;
            head = newPageNode;
        }
    }
    remainingFreePages = pages;
    return head;
}

int isPageInMemory(vector<pageNode*> &cache, int pageId, int processId)
{
    // corresponding cache mutex should be locked before calling this function.
    for(int cache_index = 0; cache_index < cache.size(); cache_index++)
    {
        if( cache[cache_index]->usingProcessId == processId && 
            cache[cache_index]->processPageId == pageId )
                return cache_index;
    }
    return -1;
}

bool removeFrameFromProcess(pageNode* page)
{
    processNode* process = processHead;
    while(process->processId != page->usingProcessId)
        process = process->next;
    if(process != NULL)
    {
        //remove the appropriate page by iterate over page list
        pageNode* curPage = process->pageHead;
        pageNode* temp = NULL; // to maintain previous process pointing to current process while iterating over process linked list
        while(curPage != page && curPage)
        {
            temp = curPage;
            curPage = curPage->next;
        }
        if(curPage != NULL)
        {
            curPage->next = freePageHead;
            freePageHead = curPage;
            if(temp != NULL)
                temp->next = curPage->next;
            else
                process->pageHead = curPage->next;
            return true;
        }
        return false;
    }
    return false;
}

void removeFramesPostProcess(processNode* process)
{
    while(process->pageHead != NULL)
    {
        pageNode* temp = process->pageHead;
        process->pageHead = process->pageHead->next;
        
        temp->next = freePageHead;
        freePageHead = temp;

        remainingFreePages++;
    }
}

/*
//used when allocating pages to a process about to be run
void allocatePage()
{
    //if there are no pages assigned to this process
    //first page is always assigned processPageId 0
    if ( processHead->nextPage == NULL)
    {
        processHead->nextPage = pageHead;
        processHead->nextPage->processPageId = 0;
    }
    else
    {
        //UNFINISHED
        //need to figure out allocating the correct processPageId?
        //will it always be 0 to memorySize or different numbers for ID
        cout << "NOT IMPLEMENTED" << endl;
    }
}
*/

int getRandomPage(int prevPage, int memorySize)
{
    // resultant random page using "locality of reference" algorithm.
    int randomPage;
    int randNum = rand()%10;
    if(randNum < 7)
        randomPage = prevPage + (rand()%3) - 1;
    else
    {
        int binRand = rand()%2;
        if(binRand == 0)
            randomPage = rand()%(prevPage-1);
        else
            randomPage = (rand()% (memorySize-prevPage-2) )+ prevPage+2;
    }
    if(0 <= randomPage && randomPage<memorySize)
        return randomPage;
    return getRandomPage(prevPage, memorySize);
}

bool fifo ()
{
    return false;
}

pthread_mutex_t lruMutex= PTHREAD_MUTEX_INITIALIZER;
vector<pageNode*> globalLruCache;
void* lru(void* curProcess)
{
    processNode* process = (processNode*)curProcess;
    int currentTime = getTimeStamp();
    process->startTime = currentTime;

    int prevPage = 0;
    while((currentTime-process->startTime) < process->serviceTime)
    {
        //sleep for 100mSec
        usleep(100000);
        int randomPage = getRandomPage(prevPage, process->memorySize);
        //grab lock
        pthread_mutex_lock(&lruMutex);
        int pageIndex = isPageInMemory(globalLruCache, randomPage, process->processId);
        if( pageIndex != -1)
        {
            // update cache
            pageNode *temp = globalLruCache[pageIndex];
            globalLruCache.erase(globalLruCache.begin() + pageIndex);
            globalLruCache.insert(globalLruCache.begin(), temp);  
            //update cache hit ----

        }
        else
        {
            if(remainingFreePages == 0)
            {
                // evict least recently used page from cache
                pageNode* temp = globalLruCache.back();
                globalLruCache.pop_back();
                // process pageHead per process which maintains list of pages currently used by that process
                if(!removeFrameFromProcess(temp))
                    cout << "potential bug in the code, trying to remove frame not used by the current process" << endl;
                remainingFreePages++;
                // update cache miss
            }
            //allocate page, update lru cache, freePagesHead, process pageHead,remainingFreePages
            freePageHead->processPageId = randomPage;
            freePageHead->usingProcessId = process->processId;
            globalLruCache.insert(globalLruCache.begin(), freePageHead);
            
            pageNode* temp = freePageHead;
            freePageHead = freePageHead->next;

            temp->next = process->pageHead;
            process->pageHead = temp;

            remainingFreePages--;

            //update cache hit
        }
        //release lruCache mutex lock
        pthread_mutex_unlock(&lruMutex);

        //update currentTime
        currentTime = getTimeStamp();
    }
    //free in-memory frames used by this process
    removeFramesPostProcess(process);
    return NULL;
}

bool lfu()
{
    return false;
}

bool mfu()
{
    return false;
}

int main()
{
    processHead = generateJobsAndSort( 150 );
    freePageHead = generateFreePageList( 100 );

    pthread_t tids[150];
    //if a process has arrived on or before timestamp, create and run thread
    processNode* curProcess = processHead;
    // only main thread is incrementing/writing into timestamp variable. so, main thread can directly read timestamp without locking.
    timeStamp = 0;
    int threadIdIndex = 0, i = 0;
    while(timeStamp<60 && curProcess)
    {
        if(curProcess->arrivalTime <= timeStamp && remainingFreePages>=4)
        {
            pthread_mutex_lock(&lruMutex);
            //initial reference : page-0
            freePageHead->processPageId = 0;
            freePageHead->usingProcessId = curProcess->processId;

            
            globalLruCache.insert(globalLruCache.begin(), freePageHead);
           

            pageNode* temp = freePageHead;
            freePageHead = freePageHead->next;
            temp->next = curProcess->pageHead;
            curProcess->pageHead = temp;

            remainingFreePages--;
            pthread_mutex_unlock(&lruMutex);
            
            pthread_create(&tids[i++], NULL, lru, curProcess);
            curProcess = curProcess->next;
            threadIdIndex++;
        }
        //sleep for a sec and update timer
        usleep(1000000); 
        pthread_mutex_lock(&timerMutex);
        timeStamp++;
        pthread_mutex_unlock(&timerMutex);
    }

    for(int i=0; i<threadIdIndex; i++)
        pthread_join( tids[i], NULL );

    return 0;
}