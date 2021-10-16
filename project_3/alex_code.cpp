#include <stdio.h>
#include <pthread.h>
#include <vector>
#include <string>
#include <iostream>
#include <unistd.h>

using namespace std;


class Concert {
    public:
    int seats[ 10 ][ 10 ];
    int seatsRemaining;

    Concert()
    {
        for ( int i = 0; i < 10; i++ )
        {
            for( int j = 0; j < 10; j++ )
            {
                seats[ i ][ j ] = -1; 
            }
        }
        seatsRemaining = 100;
    }
};
class Customer {
    public:
    int arrivalTime;
    int serviceTime;
    float turnaroundTime;
    float responseTime;
    bool inQueue;
    bool finished;

    Customer()
    {
        arrivalTime = rand() % 60; //arrivalTime between 0 and 59
        inQueue = false;
        responseTime = 0;
        finished = false;
    }
};

//compare function to sorty by arrival time
bool customerCompare( Customer x, Customer y )
{
    return x.arrivalTime < y.arrivalTime;
}

class Seller {
    public:
    char sellerType;
    vector< Customer > customerQueue;
    vector< Customer > eventQueue;
    int sellerId;
    

    Seller( char sType, int sellId, int customers )
    {
        sellerType = sType;
        sellerId = sellId;
        for ( int i = 0; i < customers; i ++ )
        {
            customerQueue.push_back( Customer() );
            //initialize service time according to the seller type
            if ( sellerId == 0 )
                customerQueue[ i ].serviceTime = rand() % 2 + 1;
            else if ( sellerId < 4 )
                customerQueue[ i ].serviceTime = rand() % 3 + 2;
            else
                customerQueue[ i ].serviceTime = rand() % 4 + 4;
        }
        sort( customerQueue.begin(), customerQueue.end(), customerCompare );

    }

};

//global Variables
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t concertMutex= PTHREAD_MUTEX_INITIALIZER;
Concert concert;
int current_time = 0;

void printConcert()
{
    for ( int i = 0; i < 10; i++ )
    {
        for ( int j = 0; j < 10; j++ )
        {
            if ( concert.seats[ i ][ j ] == -1)
                cout << "-";
            else if ( concert.seats[ i ][ j ] == 0 )
                cout << "H";
            else if ( concert.seats[ i ][ j ] < 4 )
                cout << "M";
            else
                cout << "L";
        }
        cout << endl;
    }
}
//helper function for findAndAllocateSeat, given the row, finds an empty seat
int findSeatInRow ( int row )
{
    int index = -1;
    for ( int i = 0; i < 10 ; i ++ )
    {
        if ( concert.seats[ row ][ i ] == -1 )
        {
            index = i;
            return index;
        }
    }
    return index;
}
//findAndAllocateSeat should be called when a thread has a mutex
//if a seat was found, we allocate it and return true
//takes an ID to determine the row order 
bool findAndAllocateSeat( int id )
{
    if ( id == 0 )
    {
        int rowOrder[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        for ( int i = 0; i < 10; i++ )
        {
            int row = rowOrder[ i ];
            int seat = findSeatInRow( row );
            if( seat != -1 )
            {
                concert.seats[ row ][ seat ] = id;
                concert.seatsRemaining--;
                return true;
            }
        }
    }
    else if ( id < 4 )
    {
        int rowOrder[] = { 4, 5, 3, 6, 2, 7, 1, 8, 0, 9 };
        for ( int i = 0; i < 10; i++ )
        {
            int row = rowOrder[ i ];
            int seat = findSeatInRow( row );
            if( seat != -1 )
            {
                concert.seats[ row ][ seat ] = id;
                concert.seatsRemaining--;
                return true;
            }
        }
    }
    else {
        int rowOrder[] = { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
        for ( int i = 0; i < 10; i++ )
        {
            int row = rowOrder[ i ];
            int seat = findSeatInRow( row );
            if( seat != -1 )
            {
                concert.seats[ row ][ seat ] = id;
                concert.seatsRemaining--;
                return true;
            }
        }
    }
    return false;
}
// seller thread to serve one time slice (1 minute)
void *sell( Seller seller )
{
    //index used to keep track of which Customer is being served
    int index = 0;
    //output for when a customer arrives in the queue
    for ( int i = 0; i < seller.customerQueue.size(); i++ )
    {
        if ( seller.customerQueue[ i ].arrivalTime <= current_time && !seller.customerQueue[ i ].inQueue )
        {
            cout << current_time <<  ":\tCustomer has arrived at the end of seller " << seller.sellerId << "'s queue" << endl;
            seller.customerQueue[ i ].inQueue = true;
        }
    }
    //check if there are currently customers in the event queue

    while ( current_time < 70 )
    {
        pthread_mutex_lock( &concertMutex);
        pthread_cond_wait(&cond, &concertMutex);
        for ( int i = 0; i < seller.customerQueue.size(); i++ )
        {
            //queue is empty
            if ( !seller.customerQueue[ i ].inQueue && !seller.customerQueue[ i ].finished )
            {
                pthread_mutex_unlock(&concertMutex);
                return NULL;
            }
            else
            {
                //if service time is 0, we now need to assign them a seat
                if ( !seller.customerQueue[ i ].inQueue )
                {
                    //a seat was found
                    if ( findAndAllocateSeat( seller.sellerId ) )
                    {
                        cout << current_time <<  ":\tA seat was sold by seller " << seller.sellerId << endl;
                        //turnaround time for when the customer gets the seat
                        seller.customerQueue[ i ].turnaroundTime = current_time -
                            seller.customerQueue[ i ].arrivalTime;
                        //used for calculating throughput later
                        seller.customerQueue[ i ].finished = true;
                        seller.customerQueue[ i ].inQueue = false;
                        //printConcert();
                        index++;
                    }
                    else
                    {
                        cout << current_time <<  ":\tThe concert is sold out" << endl;
                    }
                    //end thread here since a seat was assigned or concert is sold out
                    pthread_mutex_unlock(&concertMutex);
                    return NULL;
                }
                //first time a customer is serviced, we need to set the responseTime
                if ( seller.customerQueue[ i ].responseTime == 0 )
                {
                    seller.customerQueue[ i ].responseTime = current_time - 
                        seller.customerQueue[ i ].arrivalTime;
                }
                seller.customerQueue[ i ].serviceTime--;
                pthread_mutex_unlock(&concertMutex);
                return NULL;
            }    
        }
        // Serve any buyer available in this seller queue that is ready
        // now to buy ticket till done with all relevant buyers in their queue
    }
    return NULL; // thread exits
    
}
void wakeup_all_seller_threads()
{
    pthread_mutex_lock(&concertMutex);
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&concertMutex);
}


int main( int argc, char* argv[] )
{
    int i;
    pthread_t tids[10];
    char seller_type;
    int customers = 10;
    if ( argc > 1 )
        customers = atoi( argv[ 1 ] );
    //cout << customers << endl;
   
    // Create necessary data structures for the simulator.
    // Create buyers list for each seller ticket queue based on the
    // N value within an hour and have them in the seller queue.
    // Create 10 threads representing the 10 sellers.
    vector< Seller* > sellers;
    seller_type = 'H';
    sellers.push_back (  new Seller( seller_type, 0, customers ) );
    ///not exactly sure why we need (void *(*)(void *)) but compiler complains otherwise
    pthread_create( &tids[0], NULL, (void *(*)(void *)) sell, sellers[0] );
    seller_type = 'M';
    for (i = 1; i < 4; i++)
    {
        usleep( 1000 );
        sellers.push_back( new Seller( seller_type, i, customers ) );
        pthread_create(&tids[i], NULL, (void *(*)(void *)) sell , sellers[i] );
    }
    seller_type = 'L';
    for (i = 4; i < 10; i++)
    {
        usleep( 1000 );
        sellers.push_back( new Seller( seller_type, i, customers ) );
        pthread_create(&tids[i], NULL, (void *(*)(void *)) sell, sellers[i] );
    }
    // wakeup all seller threads
    //concert only goes to time 60, but loop goes longer in case customers are in progress of being serviced
    for ( current_time = 0; current_time < 70; current_time++ )
    {
        usleep( 1000 );
        wakeup_all_seller_threads();
        if ( concert.seatsRemaining == 0 )
        {
            break;
        }
    }

    //wait for all seller threads to exit
    for (i = 0 ; i < 10 ; i++)
    {
        pthread_join( tids[i], NULL );
    }
    cout << "FIN" << endl;
    // Printout simulation results
    return 0;
}