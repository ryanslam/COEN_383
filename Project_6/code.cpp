#include <stdio.h> 
#include <stdlib.h> 
#include <fcntl.h> 
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/wait.h>
#include <sys/time.h> 
#include <sys/ioctl.h> 
#include <vector>
#include <iostream>
using namespace std;
int main()
{
    vector<pid_t> pid(5);
    for(int i=0;i<5; i++)
    {
        pid[i] = fork();
        if(pid[i] == 0)
        {

            cout << "hello from child" << endl;


        exit(0);
        }
        else if(pid[i] < 0)
        {
            cout << "Fork() failed" << endl;
            exit(1);
        }
    }

    for(int i=0; i<5 ;i++)
    {
        int status;
        waitpid(pid[i], &status, 0);
    }
    return 0;

}