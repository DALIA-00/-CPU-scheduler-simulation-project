#ifndef SHEDULER_H
#define SHEDULER_H

#include "process .h"
#include <vector>
#include <queue>
using namespace std;

class scheduler {
private:  //just for sheduled class 
int currentTime ;
process * currentProcess;
priority_queue<process*> readyQueue;   // processes ready to run
queue<process*> ioQueue;  // processes performing IO
queue<process*> waitingQueue;  // processes waiting for resources

public:
scheduler (); // Constructor
void addProcess(process* p ); // Add a new process to the scheduler
void schedule(); // Main scheduling function
void executeCurrentProcess(); // Execute the current process for one time unit

void updateIO();
void updateWaiting();
void applyAging();
void checkPreemption();
void moveToIOQueue(process* p);
void moveToReadyQueue(process* p);
void moveToWaitingQueue(process* p);

process* getCurrentProcess() const;
int getCurrentTime() const ;
bool allProcessesFinished() const ;
vector<process*> getallprocesses()const;



};


#endif 