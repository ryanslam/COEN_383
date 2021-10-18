#include <stdio.h>
#include <pthread.h>
#include <vector>
#include <string>
#include <iostream>
#include <unistd.h>
#include <algorithm>

using namespace std;


class Concert {
    public:
    int seats[ 10 ][ 10 ];
    int seatsRemaining;
    //used for printing
    int customerSeats[ 10 ][ 10 ];

    Concert()
    {
        for ( int i = 0; i < 10; i++ )
        {
            for( int j = 0; j < 10; j++ )
            {
                seats[ i ][ j ] = -1; 
                customerSeats[ i ][ j ] = -1;
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
    int startTime;
    int endTime;
    bool inQueue;
    bool finished;
    int customerId;

    Customer()
    {
        arrivalTime = rand() % 60; //arrivalTime between 0 and 59
        inQueue = false;
        responseTime = -1;
        turnaroundTime = -1;
        finished = false;
        startTime = -1;
        endTime = -1;
    }
};

//compare function to sorty by arrival time
bool customerCompare( Customer* x, Customer* y )
{
    return x->arrivalTime < y->arrivalTime;
}

class Seller {
    public:
    char sellerType;
    vector< Customer* > customerQueue;
    int sellerId;
    

    Seller( char sType, int sellId, int customers )
    {
        sellerType = sType;
        sellerId = sellId;
        for ( int i = 0; i < customers; i ++ )
        {
            customerQueue.push_back( new Customer() );
            //initialize service time according to the seller type
            if ( sellerId == 0 )
                customerQueue[ i ]->serviceTime = rand() % 2 + 1;
            else if ( sellerId < 4 )
                customerQueue[ i ]->serviceTime = rand() % 3 + 2;
            else
                customerQueue[ i ]->serviceTime = rand() % 4 + 4;
        }
        sort( customerQueue.begin(), customerQueue.end(), customerCompare );
        for ( int i = 0; i < customers; i++ )
        {
            customerQueue[ i ]->customerId = i + 1;
        }

    }

};

//global Variables
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t concertMutex= PTHREAD_MUTEX_INITIALIZER;
Concert concert;
int current_time = -1;

void printConcert()
{
    cout << "CONCERT SEATING CHART" << endl;
    for ( int i = 0; i < 10; i++ )
    {
        for ( int j = 0; j < 10; j++ )
        {
            if ( concert.seats[ i ][ j ] == -1)
                cout << " || - ||";
            else if ( concert.seats[ i ][ j ] == 0 )
            {                
                cout << " |H";
                cout << concert.seats[ i ][ j ];
                if ( concert.customerSeats[ i ][ j ] < 10 )
                {
                    cout << "0";
                }
                cout << concert.customerSeats[ i ][ j ] << "| ";
                
            }
            else if ( concert.seats[ i ][ j ] < 4 )
            {
                cout << " |M";
                cout << concert.seats[ i ][ j ];
                if ( concert.customerSeats[ i ][ j ] < 10 )
                {
                    cout << "0";
                }
                cout << concert.customerSeats[ i ][ j ] << "| ";
            }
            else
            {
                cout << " |L";
                cout << concert.seats[ i ][ j ] - 3;
                if ( concert.customerSeats[ i ][ j ] < 10 )
                {
                    cout << "0";
                }
                cout << concert.customerSeats[ i ][ j ] << "| ";
            }
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
bool findAndAllocateSeat( int id, int custId )
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
                concert.customerSeats[ row ][ seat ] = custId;
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
                concert.customerSeats[ row ][ seat ] = custId;
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
                concert.customerSeats[ row ][ seat ] = custId;
                concert.seatsRemaining--;
                return true;
            }
        }
    }
    return false;
}
// seller thread to serve one time slice (1 minute)
void *sell( Seller *seller )
{    
    bool servicing = false;
    //check if there are currently customers in the event queue
    while ( current_time < 60 || servicing )
    {

        //usleep( 3000 );
        pthread_mutex_lock( &concertMutex);
        pthread_cond_wait(&cond, &concertMutex);

        //output for when a customer arrives in the queue
        for ( int i = 0; i < seller->customerQueue.size(); i++ )
        {
            if ( seller->customerQueue[ i ]->arrivalTime <= current_time && !seller->customerQueue[ i ]->inQueue && 
                !seller->customerQueue[ i ]->finished )
            {
                if ( seller->sellerId > 4 )
                    cout << current_time <<  ":\tCustomer " << seller->customerQueue[ i ]->customerId << " has arrived at the end of seller " 
                        << seller->sellerType << seller->sellerId - 3 << "'s queue" << endl;
                else
                    cout << current_time <<  ":\tCustomer " << seller->customerQueue[ i ]->customerId << " has arrived at the end of seller " 
                        << seller->sellerType << seller->sellerId << "'s queue" << endl;
                seller->customerQueue[ i ]->inQueue = true;
            }
        }

        //algorithm where service customers
        for ( int i = 0; i < seller->customerQueue.size(); i++ )
        {
            //if customer not in queue, but is finished, go to next iteration
            if ( !seller->customerQueue[ i ]->inQueue && seller->customerQueue[ i ]->finished )
            {
                continue;
            }
            else if ( seller->customerQueue[ i ]->inQueue )
            {
                //if service time is 0, we now need to assign them a seat
                if ( seller->customerQueue[ i ]->serviceTime == 0 )
                {
                    //a seat was found
                    if ( findAndAllocateSeat( seller->sellerId, seller->customerQueue[ i ]->customerId ) )
                    {
                        //usleep( 1000 );
                        if ( seller->sellerId >= 4 )
                            cout << current_time <<  ":\tA seat was sold by seller " << seller-> sellerType << seller->sellerId - 3;
                        else
                            cout << current_time <<  ":\tA seat was sold by seller " << seller->sellerType << seller->sellerId;
                        cout << " to customer " << seller->customerQueue[ i ]->customerId << endl;
                        //turnaround time for when the customer gets the seat
                        seller->customerQueue[ i ]->turnaroundTime = current_time -
                            seller->customerQueue[ i ]->arrivalTime;
                        //used for calculating throughput later
                        seller->customerQueue[ i ]->finished = true;
                        //remove customer from queue if 
                        seller->customerQueue[ i ]->inQueue = false;
                        seller->customerQueue[ i ]->endTime = current_time; 
                        servicing = false;
                        printConcert();
                        //pthread_mutex_unlock(&concertMutex);
                    }
                    else
                    {
                        //usleep( 1000 );
                        //also set end time here so we can keep track of final throughput
                        if ( seller->sellerId >= 4 )
							cout << current_time <<  ":\tThe conert is sold out, " << "customer " << seller->customerQueue[ i ]->customerId << " of seller " << seller-> sellerType << seller->sellerId -3 << " was rejected " << endl;
                        else
							cout << current_time <<  ":\tThe conert is sold out, " << "customer " << seller->customerQueue[ i ]->customerId << " of seller " << seller-> sellerType << seller->sellerId << " was rejected " << endl;
                        seller->customerQueue[ i ]->endTime = current_time; 
                        cout << current_time <<  ":\tThe concert is sold out" << endl;
                        pthread_mutex_unlock(&concertMutex);
                        //printConcert();
                        return NULL;
                    }
                }
                else
                {
                    //first time a customer is serviced, we need to set the responseTime
                    if ( seller->customerQueue[ i ]->responseTime == -1 )
                    {
                        seller->customerQueue[ i ]->responseTime = current_time - 
                            seller->customerQueue[ i ]->arrivalTime;
                        seller->customerQueue[ i ]->startTime = current_time;
                        servicing = true;
                    }
                    seller->customerQueue[ i ]->serviceTime--;
                    //pthread_mutex_unlock(&concertMutex);  
                }
                break;
            }
        }
        pthread_mutex_unlock(&concertMutex);
    }
    // Serve any buyer available in this seller queue that is ready
    // now to buy ticket till done with all relevant buyers in their queue

    return NULL; // thread exits
    
}
void wakeup_all_seller_threads()
{
    pthread_mutex_lock(&concertMutex);
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&concertMutex);
}

void printMetrics( vector< Seller* > sellers)
{
    float avgTurn = 0;
    float turn = 0;
    float resp = 0;
    float avgResp = 0;
    float avgThroughput = 0;
    float endTime = 0;
    float customers = 0;
    float totalCustomersServed = 0;
    float totalCustomers = 0;
    for( int i = 0; i < sellers.size(); i ++ )
    {
        customers = 0;
        turn = 0;
        resp = 0;
        for ( int j = 0; j < sellers[ i ]->customerQueue.size(); j++ )
        {
            if ( sellers[ i ]->customerQueue[ j ]->finished )
            {
                turn += sellers[ i ]->customerQueue[ j ]->turnaroundTime;
                resp += sellers[ i ]->customerQueue[ j ]->responseTime;
                customers++;
                //endtime set this way since customerQueue is organized by arrivalTime
                //therefore the last finished customer has the highest endtime
                endTime = sellers[ i ]->customerQueue[ j ]->endTime;
                totalCustomersServed++;
            }
            totalCustomers++;
        }
        avgTurn += turn / customers;
        avgResp += resp / customers;
        avgThroughput += customers / endTime;
        if( i == 0)
        {
            cout << "Metrics for seller type " << sellers[i]->sellerType << endl;
            cout << "Average Turnaround Time: " << avgTurn << endl;
            cout << "Average Response Time: " << avgResp << endl;
            cout << "Average Throughput: " << avgThroughput << endl;
            cout << "Customers Served: " << totalCustomersServed << endl;
            cout << "Total Customers: " << totalCustomers << endl;
            cout << "Average Customers Turned Away: " << totalCustomers - totalCustomersServed << endl;
            cout << endl;
            avgTurn = 0;
            avgResp = 0;
            avgThroughput = 0;
            totalCustomersServed = 0;
            totalCustomers = 0;


            
        }
        else if( i == 3)
        {
            cout << "Metrics for seller type " << sellers[i]->sellerType << endl;
            cout << "Average Turnaround Time: " << avgTurn / 3 << endl;
            cout << "Average Response Time: " << avgResp / 3 << endl;
            cout << "Average Throughput: " << avgThroughput / 3 << endl;
            cout << "Customers Served: " << totalCustomersServed << endl;
            cout << "Total Customers: " << totalCustomers << endl;
            cout << "Average Customers Turned Away: " << ( totalCustomers - totalCustomersServed ) / 3 << endl;
            cout << endl;
            avgTurn = 0;
            avgResp = 0;
            avgThroughput = 0;
            totalCustomersServed = 0;
            totalCustomers = 0;
        }
        else if( i == 9 )
        {
            cout << "Metrics for seller type " << sellers[i]->sellerType << endl;
            cout << "Average Turnaround Time: " << avgTurn / 6 << endl;
            cout << "Average Response Time: " << avgResp / 6 << endl;
            cout << "Average Throughput: " << avgThroughput / 6 << endl;
            cout << "Customers Served: " << totalCustomersServed << endl;
            cout << "Total Customers: " << totalCustomers << endl;
            cout << "Average Customers Turned Away: " << ( totalCustomers - totalCustomersServed ) / 6 << endl;
            cout << endl;
            avgTurn = 0;
            avgResp = 0;
            avgThroughput = 0;
            totalCustomersServed = 0;
            totalCustomers = 0;
        } 
    }

}

int main( int argc, char* argv[] )
{
    //srand( 1 );
    int i;
    pthread_t tids[10];
    char seller_type;
    int customers = atoi( argv[ 1 ] );   
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
    for ( current_time = -1; current_time < 120; current_time++ )
    {
        usleep( 5000 );
        wakeup_all_seller_threads();
    }
    //wait for all seller threads to exit
    for (i = 0 ; i < 10 ; i++)
    {
        pthread_join( tids[i], NULL );
    }
    cout << endl;
    printConcert();
    cout << endl;
    printMetrics( sellers );    
    // Printout simulation results
    return 0;
}