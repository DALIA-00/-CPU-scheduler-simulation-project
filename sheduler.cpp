#include "scheduler.h"
#include <iostream>
#include <algorithm>
using namespace std;

scheduler :: scheduler() {
currentTime = 0;
currentProcess = nullptr;

}
void scheduler :: addProcess(process* p ) {
    if (p == nullptr) return; 
    p->state = READY;
    readyQueue.push(p);
   // readyQueue.sort(
cout << "Time " << currentTime << ": process " << p->pid <<arrived (priority << )
        



}
