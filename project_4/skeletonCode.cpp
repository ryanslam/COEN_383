#include <iostream>
#include <stdlib.h>
#include <unistd.h>
using namespace std;

int mem[ 4 ] = { 5, 11, 17, 31 };
int service[ 5 ] = { 1, 2, 3, 4, 5 };

struct pageNode {
    //page ID so we can keep track of where pages are going
    int pageId;
    //processPageId is per process from 0 to memorySize
    int processPageId;
    pageNode* next;
};

struct processNode {
    int memorySize;
    int serviceTime;
    int processID;
    int arrivalTime;
    int startTime;
    processNode* next;
    pageNode* pageHead;

    processNode() : memorySize(mem[ (rand() % 4) ]),
                    serviceTime(service[ (rand() % 5) ]),
                    arrivalTime((rand() % 60)),
                    startTime(-1),
                    next(NULL),
                    pageHead(NULL){}
};

//Global Variables
pageNode* freePageHead;
int remainingFreePages = 0;
processNode* processHead;
int timeStamp = 0;
pthread_mutex_t timerMutex= PTHREAD_MUTEX_INITIALIZER;

void runTimer(int sec)
{
    for(int i=0; i<sec; i++)
    {
        usleep(1000000); //sleep for a sec and update timer
        pthread_mutex_lock(&timerMutex);
        timeStamp++;
        pthread_mutex_unlock(&timerMutex);
    }
}

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
    processNode* newProcessNode;
    processNode* head = NULL;
    //create unsorted process list 
    for( int i = 0; i < jobs; i++ )
    {
        newProcessNode = new processNode();
        newProcessNode->processID = i;
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
        newPageNode->pageId = i;
        if(!head) head = newPageNode;
        else{
            newPageNode->next = head;
            head = newPageNode;
        }
    }
    remainingFreePages = pages;
    return head;
}

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

vector<> globalLruCache;
bool lru(void* process)
{
    process = (processNode*)process;
    int currentTime = getTimeStamp();
    process->startTime = currentTime;


    int prevPage = 0;
    while((currentTime-process->startTime) < process->serviceTime)
    {
        //sleep for 100mSec
        usleep(100000);
        int randomPage = getRandomPage(prevPage, process->memorySize);
        referencePage(randomPage, process);
        //grab lock
        // if in memory ?
            // update cache, cache hit
        //else
        if(globalLruCache.size() == 100)
        {
            // evict least recently used page from cache, update freePagesHead, 
            // process pageHead, remainingFreePages
            // update cache miss
        }
        //allocate page, update lru cache, freePagesHead, process pageHead,remainingFreePages, cache hit
        //release lock
        currentTime = getTimeStamp();
    }
    //free in-memory pages used by this process
    postProcess();
    return false;
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
    
    //if a process has arrived on or before getTimeStamp(), create and run thread

    return 0;
}