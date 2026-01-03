#include "process .h"
#include <string>
#include <vector>
using namespace std;

process::process(int id, int arrival, int prio) {
    this->pid = id;
    this->arrivalTime = arrival;
    this->priority = prio;
    this->waitingTime = 0;
    this->burstIndex = 0;
    this->state = NEW;
}

bool process::finished() const {
    return burstIndex >= bursts.size();
}

Burst process::currentBurst() const {
    if (burstIndex < bursts.size()) {
        return bursts[burstIndex];
    }
   
    Burst empty;
    empty.type = CPU;
    return empty;
}

void process::nextBurst() {
    burstIndex++;
}