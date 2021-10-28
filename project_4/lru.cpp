#include <iostream>
#include <fstream>
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
    double serviceTime; 
    int processId;
    int arrivalTime;
    int startTime;
    bool started;
    bool finished;
    int prevPageReferenced;
    processNode* next;
    pageNode* pageHead;

    processNode() : arrivalTime((rand() % 60)),
                    startTime(-1),
                    started(false),
                    finished(false),
                    prevPageReferenced(-1),
                    next(NULL),
                    pageHead(NULL){}
};

//Global Variables
pageNode* freePageHead;
int remainingFreePages = 100;
processNode* processHead;
vector<pageNode*> lruCache;
vector<int> memMap(100, -1);
double avgHitByMissRatio = 0.0;
int processessSwappedIn = 0;

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
            if(temp != NULL)
                temp->next = curPage->next;
            else
                process->pageHead = curPage->next;

            curPage->next = freePageHead;
            freePageHead = curPage;

            remainingFreePages++;

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

        for(int lru_index = 0; lru_index < lruCache.size(); lru_index++)
        {
            if(lruCache[lru_index] == temp)
            {
                lruCache.erase(lruCache.begin() + lru_index);
                memMap[ temp->frameId ] = -1;
                cout << "freeing frame id : " << temp->frameId << " as process : " << temp->usingProcessId << " has finished" << endl;
                break;
            }
        }
    }
    cout << "Remaining free pages : " << remainingFreePages << endl;
}

int getRandomPage(int prevPage, int memorySize)
{
    // resultant random page using "locality of reference" algorithm.
    int randomPage;
    int randNum = rand()%10;
    if(randNum < 7)
    {
        randomPage = prevPage + (rand()%3) - 1;
        if(randomPage < 0)
            randomPage = memorySize - 1;
        else if(randomPage == memorySize)
            randomPage = 0;
    }
    else
    {
        if( prevPage-1 <=0)
            randomPage = (rand()% (memorySize-prevPage-2) )+ prevPage+2;
        else if((memorySize-prevPage-2) <= 0)
            randomPage = rand()%(prevPage-1);
        else
        {
            int binRand = rand()%2;
            if(binRand == 0 )
                randomPage = rand()%(prevPage-1);
            else
                randomPage = (rand()% (memorySize-prevPage-2) )+ prevPage+2;
        }
    }
    return randomPage;
}

void printMemMap()
{
    for( int i = 0; i < memMap.size() ; i++ )
    {
        if ( i % 10 == 0 && i != 0 )
            cout << endl;
        if ( memMap[ i ] == - 1 )
            cout << "|...|";
        else if (memMap[ i ] < 10)
            cout << "|00" << memMap[ i ] <<  "|";
        else if(memMap[ i ] < 100)
            cout << "|0" << memMap[ i ] <<  "|";
        else
            cout << "|" << memMap[ i ] <<  "|";
        
    }
    cout << endl;
}

void lru(int pageRefCount)
{
    int hitCount = 0;
    int missCount = 0;

    for(int timeStamp = 0; timeStamp < 600; timeStamp++)
    {
        for(processNode *curProcess = processHead; curProcess != NULL; curProcess = curProcess->next)
        {
            if(pageRefCount == 0)
                return;

            if(!curProcess->started && curProcess->arrivalTime <= timeStamp/10 && remainingFreePages >= 4)
            {
                // initial page reference : page-0
                curProcess->started = true;

                cout << "timestamp: " << timeStamp/10 << "." << timeStamp%10;
                cout << " Process ID: " << curProcess->processId << " Entered ";              
                cout << " Size: " << curProcess->memorySize;
                cout << " Service Time: " << curProcess->serviceTime << endl;

                remainingFreePages--;

                cout << "Allocated new frame id: " << freePageHead->frameId << " to process id: " << curProcess->processId << endl; 

                pageNode* temp = freePageHead;
                freePageHead = freePageHead->next;

                temp->processPageId = 0;
                temp->usingProcessId = curProcess->processId;

                temp->next = curProcess->pageHead;
                curProcess->pageHead = temp;

                lruCache.insert(lruCache.begin(), temp);
                memMap[ lruCache[0]->frameId ] = curProcess->processId;

                cout << "< timestamp: " << timeStamp/10 << "." << timeStamp%10;
                cout << ", Process ID: " << curProcess->processId;
                cout << ", virtual page referenced: " << 0;
                cout << ", Frame Id in Memory:  " << temp->frameId;
                cout << ", No Page Evicted " << endl; 

                processessSwappedIn++;
                printMemMap();
                continue;
            }

            if(curProcess->started && !curProcess->finished)
            {
                // make a page reference
                int randomPage = getRandomPage(curProcess->prevPageReferenced, curProcess->memorySize);
                pageRefCount--;

                int pageIndexIncache = isPageInMemory(lruCache, randomPage, curProcess->processId);
                if(pageIndexIncache != -1)
                {
                    // referenced page is in memory
                    pageNode* temp = lruCache[pageIndexIncache];
                    lruCache.erase(lruCache.begin() + pageIndexIncache);
                    lruCache.insert(lruCache.begin(), temp);

                    cout << "< timestamp: " << timeStamp/10 << "." << timeStamp%10;
                    cout << ", Process ID: " << curProcess->processId;
                    cout << ", virtual page referenced: " << randomPage;
                    cout << ", Frame Id in Memory:  " << temp->frameId;
                    cout << ", No Page Evicted " << endl; 

                    hitCount++;
                }
                else
                {
                    missCount++;
                    hitCount++;
                    int pageEvictedFromProcessId = -1;
                    int evictedVirtualPageId = -1;
                    if(remainingFreePages == 0)
                    {
                        removeFrameFromProcess(lruCache.back());
                        memMap[ lruCache.back()->frameId ] = -1;
                        lruCache.pop_back();

                        cout << "MEMORY FULL!!" << "\tEVICTED " << lruCache.back()->usingProcessId << "'s virtual page " << lruCache.back()->processPageId << endl;
                        pageEvictedFromProcessId = lruCache.back()->usingProcessId;
                        evictedVirtualPageId = lruCache.back()->processPageId;
                    }

                    remainingFreePages--;

                    cout << "Allocated new frame id: " << freePageHead->frameId << " to process id: " << curProcess->processId << endl; 

                    pageNode* temp = freePageHead;
                    freePageHead = freePageHead->next;

                    temp->processPageId = randomPage;
                    temp->usingProcessId = curProcess->processId;

                    temp->next = curProcess->pageHead;
                    curProcess->pageHead = temp;

                    lruCache.insert(lruCache.begin(), temp);
                    memMap[ lruCache[0]->frameId ] = curProcess->processId;

                    cout << "< timestamp: " << timeStamp/10 << "." << timeStamp%10;
                    cout << ", Process ID: " << curProcess->processId;
                    cout << ", virtual page referenced: " << randomPage;
                    cout << ", Frame Id in Memory:  " << temp->frameId;

                    if(pageEvictedFromProcessId != -1 && evictedVirtualPageId != -1)
                        cout << ", EVICTED process" << pageEvictedFromProcessId << "'s virtual page " << evictedVirtualPageId << " >" << endl;
                    else
                        cout << ", No Page Evicted >" << endl; 
                }
                curProcess->serviceTime -= 0.1;
                if(curProcess->serviceTime <= 0)
                {
                    curProcess->finished = true;

                    cout << "timestamp: " << timeStamp/10 << "." << timeStamp%10;
                    cout << " Process ID: " << curProcess->processId << " has finished " << endl;

                    removeFramesPostProcess(curProcess);
                    printMemMap();
                }
            }
        }
    }
    avgHitByMissRatio += (hitCount*1.0) / ((missCount + hitCount) * 1.0) ;
}

int main()
{
    std::ofstream out("LRU_latest_output.txt");
    std::streambuf *coutbuf = std::cout.rdbuf(); //save old buf
    std::cout.rdbuf(out.rdbuf()); //redirect std::cout to out.txt!

    for(int j=0; j<5; j++)
    {
        cout << endl << "******************* starting new run : " << j+1 << " *******************" << endl << endl;
        processHead = generateJobsAndSort( 150 );
        freePageHead = generateFreePageList( 100 );
        for(int i=0; i<memMap.size(); i++)
            memMap[i] = -1;
        lruCache.clear();
        lru(-1);
    }
    avgHitByMissRatio = avgHitByMissRatio/5.0;
    cout << "Average Hit/Miss ratio : " << avgHitByMissRatio << " (" << (avgHitByMissRatio*100.0) << "%)" << endl;
    cout << "Average Number of processess successfully swapped in : " << processessSwappedIn/5.0 << endl;

    cout << endl << "************************ running simulation for 100 page references ************************" << endl << endl;

    processHead = generateJobsAndSort( 150 );
    freePageHead = generateFreePageList( 100 );
    for(int i=0; i<memMap.size(); i++)
        memMap[i] = -1;
    lruCache.clear();
    lru(100);

    return 0;
}