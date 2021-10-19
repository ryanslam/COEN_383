#include <iostream>

using namespace std;

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
    int processName;
    int arrivalTime;
    processNode* next;
    pageNode* nextPage;
};

class linkedList {
    public:
    pageNode* pageHead;
    pageNode* pageTail;
    processNode* processHead;
    processNode* processTail;
    linkedList()
    {
        pageHead = NULL;
        pageTail = NULL;
        processHead = NULL;
        processTail = NULL;
    }

    void generateJobsAndSort( int jobs )
    {
        int mem[ 4 ] = { 5, 11, 17, 31 };
        int service[ 5 ] = { 1, 2, 3, 4, 5 };
        processNode* nextNode;
        //create unsorted process list 
        for( int i = 0; i < jobs; i++ )
        {
            nextNode = new processNode;
            nextNode->memorySize = mem[ i % 4 ];
            nextNode->serviceTime = service[ i % 5 ];
            nextNode->arrivalTime = rand() % 60;
            nextNode->processName = i;
            nextNode->next = NULL;
            nextNode->nextPage = NULL;
            //if current head is not initialized
            if ( i == 0 )
            {
                processHead = nextNode;
                processTail = processHead;
            }
            //iterate through the linked list to become the tail
            else
            {
                
                processTail->next = nextNode;
                processTail = processTail->next;

            }
        }
        // issue with sorting algorithm somehow
        // sorting algorithm by arrivalTime
        // sort will always make sure that the lowest arrival time is at the front
        processNode* cur1 = processHead;
        processNode* cur2 = processHead;
        for( int i = 0; i < jobs; i++ )
        {
            for( int j = 0; j < jobs; j++ )
            {
                //if a node has a lower arrival time, swap the node values
                //dont worry about next since we are swapping the values
                //dont worry about nextPage since we have not started running anything
                if (cur2 -> next != NULL )
                    cur2 = cur2->next;
                if ( cur1->arrivalTime > cur2->arrivalTime )
                {
                    int tempMemory = cur1->memorySize;
                    int tempService = cur1->serviceTime;
                    int tempArrival = cur1->arrivalTime;
                    int tempName = cur1->processName;
                    cur1->memorySize = cur2->memorySize;
                    cur1->serviceTime = cur2->serviceTime;
                    cur1->arrivalTime = cur2->arrivalTime;
                    cur1->processName = cur2->processName;
                    cur2->memorySize = tempMemory;
                    cur2->serviceTime = tempService;
                    cur2->arrivalTime = tempArrival;
                    cur2->processName = tempName;
                }
            }
            cur1 = cur1->next;
            cur2 = cur1;
        }
    }

    void generateFreePageList( int pages )
    {
        pageNode* p;
        for ( int i = 0; i < pages; i++ )
        {
            p = new pageNode;
            p->pageId = i;
            if ( i == 0)
            {
                pageHead = p;
                pageTail = p;
            }
            else
            {
                pageTail->next = p;
                pageTail = pageTail->next;
            }
        }
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
};




bool fifo ()
{
    return false;
}

bool lru()
{
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
    linkedList processList;
    processList.generateJobsAndSort( 150 );
    processList.generateFreePageList( 100 );
    //print statement loop to show that its sorted
    for( processNode* cur = processList.processHead; cur != NULL; cur = cur->next )
    {
        cout << "Process Name: " << cur -> processName << endl;
        cout << cur -> arrivalTime << endl;
    }
    
    return 0;
}