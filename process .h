
#ifndef PROCESS_H        // include guard - if not defined before
#define PROCESS_H
#include <string>
#include <vector>
using namespace std;
enum processState { NEW, READY, RUNNING, WAITING, TERMINATED };
enum BurstType { CPU, IO};      // Added R,F burst type , each one is diferent burst in duration , 

enum OperationType { EXEC, REQ, REL };      // Added R,F burst type , each one is diferent burst in duration , 

struct Operation {
    OperationType type;
    int resourceID;         // ID of the resource
    int amount;             // Amount to request or release
    Operation(OperationType t, int rid, int amt) : type(t), resourceID(rid), amount(amt) {}
};

struct CPUBurst {           // why not duration added ?? 
    vector <Operation> operations;                      // List of operations in this burst
    int currentOpInd= 0;                 // Index of the current operation
    
    int remainingTime;                  // in milliseconds
    CPUBurst() : currentOpInd(0), remainingTime(0) {} /// whyyy only this ,what is after : 

};
struct IOBurst {
    int duration;       // in milliseconds
    IOBurst() : duration(0) {}
};
struct Burst {       //union constructure to avoid inheriting both cpu and io burst
    BurstType type;
    CPUBurst cpuBurst;
    IOBurst ioBurst;
    Burst() : type(CPU) {}      // intitialize type to CPU by default
};
class process {
public:
    int pid;            // Process ID
    int arrivalTime;         // in milliseconds
    int priority ;
    int waitingTime;
    int burstIndex;

    processState state; 
    vector<Burst> bursts;       // List of CPU and IO bursts
    

    process(int id, int arrival, int priority);
    bool finished() const;
    Burst currentBurst() const;   // const to indicate it doesn't modify the object
    void nextBurst();
}; 
#endif      // PROCESS_H

/*
questions.. in the header file we define the structure of the class process 
and its members and methods only  right ?
what is the constructors here every where , why it is written like this :  process() : member(value) {}
Burst& currentBurst();  what is & here pointer
why duration of the burst not added in the CPU burst structure ?
why there is structures with variables and others withouts >>>> 
what is the matter with Burst strucure here having both CPUBurst and IOBurst as members ?
what are the ifndef and endif for ?
constructor used here to intialiqze the members of the structures
*/ 