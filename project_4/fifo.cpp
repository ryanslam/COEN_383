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

    pageNode() : frameId(-1),
                 processPageId(-1),
                 usingProcessId(-1),
                 next(NULL){}
};

struct processNode {
    int memorySize;
    int serviceTime;
    int processId;
    int arrivalTime;
    int startTime;
    int previousPageId;
    bool finished;
    processNode* next;
    pageNode* pageHead;

    processNode() : arrivalTime((rand() % 60)),
                    startTime(-1),
                    next(NULL),
                    pageHead(NULL),
                    finished(false),
                    previousPageId(-1){}
};

//Global Variables
pageNode* freePageHead;
int remainingFreePages = 100;
processNode* processHead;
int timeStamp = 0;
pthread_mutex_t timerMutex= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t freePageHeadMutex= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t processHeadMutex= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lruMutex= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fifoMutex= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
vector<pageNode*> globalLruCache;
//cant use queue, need to use vector so we can remove from middle of list
vector<int> fifoCache;
pageNode* firstPage;

void* runTimer(void* sec)
{
    int *psec = (int*)(sec);
    int seconds = *psec;
    for(int i=0; i<seconds; i++)
    {
        usleep(10000); //sleep for a sec and update timer
        pthread_mutex_lock(&timerMutex);
        timeStamp++;
        pthread_mutex_unlock(&timerMutex);
    }
    return NULL;
}

int getTimeStamp()
{
    int ts;
    pthread_mutex_lock(&timerMutex);
    ts = timeStamp;
    pthread_mutex_unlock(&timerMutex);
    return ts;
}

//will use for FIFO
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

//will use for FIFO
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

//will use for FIFO
pageNode* generateFreePageList( int pages )
{
    pageNode* newPageNode;
    pageNode* head = NULL;
    pageNode* tail = head;
    for ( int i = 0; i < pages; i++ )
    {
        newPageNode = new pageNode();
        newPageNode->frameId = i;
        if(!head)
        { 
            head = newPageNode;
            tail = head;
        }
        else{
            tail->next = newPageNode;
            tail = tail->next;
        }
    }
    remainingFreePages = pages;
    return head;
}

//returns process id of the next page we want to reference
int getRandomPage(int prevPage, int memorySize)
{
    // resultant random page using "locality of reference" algorithm.
    int randomPage;
    int randNum = rand()%10;
    if(randNum < 7)
    {
        randomPage = prevPage + (rand()%3) - 1;
        if ( randomPage == -1 )
        {
            //wrapping around to the max page if we go negative
            randomPage = memorySize - 1;
        }
    }
    else
    {
        int binRand = rand()%2;
        if(binRand == 0)
            randomPage = rand()%(prevPage-1);
        else
            randomPage = (rand()% (memorySize-prevPage-2) )+ prevPage+2;
    }
    return randomPage;
}


//allocates a page to the process
//removes the current head of the free pages adds that as the head of the pages a process owns 
//MUST HAVE FREE PAGES
void allocatePage( pageNode** processPageHead, int id )
{
    //need to check if processPageHead is not NULL
    pageNode* tempHead = freePageHead;
    //keeping track of the page order
    fifoCache.push_back( tempHead->frameId );
    if ( freePageHead->next != NULL)
        freePageHead = freePageHead->next;
    tempHead->next = *processPageHead;
    tempHead->usingProcessId = id;
    *processPageHead = tempHead;
    remainingFreePages--; 
}

//this should only be called when there are no free pages currently in the freePageHead
void findAndRemovePage()
{
    //iterate through each process
    for( processNode* curProcess = processHead; curProcess != NULL; curProcess = curProcess->next )
    {
        for( pageNode* curPage = curProcess->pageHead; curPage != NULL; curPage = curPage->next )
        {
            //if the next page is the page we want to remove
            //need to account for when a page only has one page in the pageHead
            if( curPage->next == NULL && curPage->frameId == fifoCache.front())
            {
                freePageHead = new pageNode;
                freePageHead->frameId = curPage->frameId;
                freePageHead->next = NULL;
                freePageHead->usingProcessId = -1;
                freePageHead->processPageId = -1;
                cout << " Page "<< freePageHead->frameId;
                cout << " Evicted from " << freePageHead->usingProcessId << endl;
                fifoCache.erase( fifoCache.begin() );
                cout << " HEREREREERE" << freePageHead->frameId << endl;
                curPage = NULL;
                remainingFreePages++;
                cout << " HEREREREERE" << freePageHead->frameId << endl;
                return;
            }
            else if ( curPage->next->frameId == fifoCache.front() )
            {
                freePageHead = new pageNode;
                freePageHead->frameId = curPage->next->frameId;
                freePageHead->next = NULL;
                freePageHead->usingProcessId = -1;
                freePageHead->processPageId = -1;
                //making sure to set the next pointer correctly
                pageNode* tempPage = curPage->next->next;
                //set the removed node to NULL
                curPage->next = NULL;
                curPage->next = tempPage;
                cout << " Page "<< freePageHead->frameId;
                cout << " Evicted from " << freePageHead->usingProcessId << endl;
                cout << curPage->next->frameId << endl;
                
                fifoCache.erase( fifoCache.begin() );
                remainingFreePages++;
                return;
            }
        }
    }
}

bool fifo()
{
    //processNode* process = ( processNode* ) curProcess;
    //lock on every time update
    //1000 msec in a second, 60,000 msec in a minute
    //updates every 100 msec, so 60,000/100 = 6000 updates
    //1 update = 100 msec
    while( timeStamp < 600 )
    {
        for ( processNode* cur = processHead; cur != NULL; cur = cur->next)
        {
            //if a process hasn't started yet, we add it to the list, and allocate pages
            //each second we will check for a new process to run
            if ( timeStamp % 10 == 0 )
            {
                if ( cur->startTime == -1 && remainingFreePages >= 4 && cur->arrivalTime <= timeStamp / 10 )
                {
                    cur->startTime = timeStamp / 10;
                    cout << "Time: " << cur->startTime;
                    cout << " Process ID: " << cur->processId;
                    cout << " Entered ";
                    cout << " Size: " << cur->memorySize;
                    cout << " Service Time: " << cur->serviceTime << endl;
                    //also need to print the memory map here, not sure what that will look like yet
                    cur->startTime = timeStamp;
                    allocatePage( &cur->pageHead, cur->processId );
                    //processes always start with page id of 0
                    cur->pageHead->processPageId = 0;
                    //need this for random page function
                    cur->previousPageId = 0;
                    //after allocating page, we consider this the 0th iteration, so we dont want to make a memory reference
                    continue;
                }
                if( cur->serviceTime > 0 )
                {
                    cur->serviceTime--;
                    if ( cur->serviceTime == 0)
                    {
                        //removeALLPAGES;
                        //set process as finished, take all of the pages away and put it back to free list
                        cur->finished = true;
                    }
                }
            }
            //check if the process is running
            if( cur->finished == true)
            {
                continue;
            }
            else if ( cur->startTime >= 0 )
            {
                //make a memory access 
                int pageId = getRandomPage( cur->previousPageId, cur->memorySize );
                //need to check if  pageId exists in the process
                bool inMemory = false;
                for( pageNode* pageTracker = cur->pageHead; pageTracker != NULL; pageTracker = pageTracker->next)
                {
                    if ( pageTracker->processPageId == pageId )
                        inMemory = true;
                }
                //if the page was in memory, i.e. a process has it, we dont have to do anything
                if ( inMemory == true )
                {
                    cur->previousPageId = pageId;
                    cout << "Time: " << timeStamp / 10;
                    cout << " Process ID: " << cur->processId;
                    cout << " Referenced Page in Memory:  " << pageId;
                    cout << " No Page Evicted " << endl;
                    
                }
                else
                {
                    cout << "Time: " << timeStamp / 10 << "." << timeStamp % 10;
                    cout << " Process ID: " << cur->processId;
                    cout << " Referenced Page Not in Memory:  " << pageId;
                    //if page not in memory, check if there are any free pages
                    if ( remainingFreePages == 0 )
                    {
                        //no free pages, so we do FIFO
                        //something wrong with this function
                        findAndRemovePage();
                        allocatePage( &cur->pageHead, cur->processId );
                    }
                    else
                    {
                        //there are free pages, so freely allocate page
                        allocatePage( &cur->pageHead, cur->processId );
                        cout << " Free Page Added to Process " << endl;
                    }
                }
            }
            else
            {
                //end the for loop when we get to a process that isnt running or finished
                break;
            }     
        }
        timeStamp++;
    }
    return false;
}

int main()
{
    processHead = generateJobsAndSort( 150 );
    freePageHead = generateFreePageList( 100 );
    fifo();
    //pthread_t tids[150];
    //if a process has arrived on or before timestamp, create and run thread
    //processNode* curProcess = processHead;

    // pthread_t timer;
    // 60 seconds in min = 60,000 microseconds
    // 60,000/100 microseconds = 6000 updates
    //int updates = 6000; 
    // pthread_create(&timer, NULL, runTimer, (void*)&sec);


    //int curTime = getTimeStamp();
    int threadIdIndex = 0, i = 0;
    // while(timeStamp < updates && curProcess)
    // {
    //     while( curProcess && curProcess->arrivalTime  <= timeStamp && timeStamp < updates)
    //     {
    //         if(remainingFreePages>=4)
    //         {              
    //             //initial reference : page-0
    //             pthread_mutex_lock(&freePageHeadMutex);

    //             remainingFreePages--;
                
    //             freePageHead->processPageId = 0;
    //             freePageHead->usingProcessId = curProcess->processId;

    //             //taking the first page from free pages
    //             pageNode* temp = freePageHead;
    //             freePageHead = freePageHead->next;

    //             pthread_mutex_unlock(&freePageHeadMutex);

    //             pthread_mutex_lock(&fifoMutex);
    //             //globalLruCache.insert(globalLruCache.begin(), temp);
    //             pthread_mutex_unlock(&fifoMutex);

    //             //making temp the new pageHead for the process
    //             //temp was the head of the free pages earlier
    //             pthread_mutex_lock(&processHeadMutex);
    //             temp->next = curProcess->pageHead;
    //             curProcess->pageHead = temp;
    //             pthread_mutex_unlock(&processHeadMutex);
                
    //             //create thread to keep track of the current process
    //             pthread_create(&tids[i++], NULL, fifo, (void*)curProcess);
    //             curProcess = curProcess->next;
    //             threadIdIndex++;
    //         }
    //         wakeup_all_process_threads();
    //         usleep( 10000 );
    //         timeStamp++;
    //         //curTime = getTimeStamp();
    //     }
    //     //curTime = getTimeStamp();
    // }

    // for(int i=0; i<threadIdIndex; i++)
    //     pthread_join( tids[i], NULL );

    return 0;
}