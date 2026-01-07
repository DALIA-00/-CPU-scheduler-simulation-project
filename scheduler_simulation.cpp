#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <algorithm>
#include <iomanip>

using namespace std;

// ==================== DATA STRUCTURES ====================

enum ProcessState { NEW, READY, RUNNING, WAITING, IO, TERMINATED };
enum OperationType { EXEC, REQUEST, RELEASE };

struct Operation {
    OperationType type;
    int resourceID;
    int amount;
    int duration;

    Operation(OperationType t, int dur) : type(t), resourceID(0), amount(0), duration(dur) {}
    Operation(OperationType t, int rid, int amt) : type(t), resourceID(rid), amount(amt), duration(0) {}
};

struct Process {
    int pid;
    int arrivalTime;
    int priority;
    int originalPriority;
    ProcessState state;

    vector<vector<Operation>> bursts;  // CPU and IO bursts
    vector<bool> isCPUBurst;           // true if CPU burst, false if IO burst

    int currentBurstIndex;
    int currentOperationIndex;
    int remainingTime;

    int timeInReadyQueue;
    int startTime;
    int completionTime;
    int waitingTime;
    int turnaroundTime;

    map<int, int> heldResources;  // resourceID -> amount held
    int waitingForResource;
    int waitingForAmount;

    Process() : pid(0), arrivalTime(0), priority(0), originalPriority(0), state(NEW),
                currentBurstIndex(0), currentOperationIndex(0), remainingTime(0),
                timeInReadyQueue(0), startTime(-1), completionTime(0), waitingTime(0),
                turnaroundTime(0), waitingForResource(-1), waitingForAmount(0) {}
};

struct GanttEntry {
    int pid;
    int startTime;
    int endTime;
};

struct ResourceInfo {
    int totalInstances;
    int availableInstances;

    ResourceInfo() : totalInstances(0), availableInstances(0) {}
    ResourceInfo(int total) : totalInstances(total), availableInstances(total) {}
};

// ==================== GLOBAL VARIABLES ====================

map<int, ResourceInfo> resources;
vector<Process> processes;
queue<int> readyQueue;
queue<int> ioQueue;
queue<int> waitingQueue;
int currentProcess = -1;
int currentTime = 0;
vector<GanttEntry> ganttChart;
int TIME_QUANTUM = 10;

// ==================== UTILITY FUNCTIONS ====================

string stateToString(ProcessState state) {
    switch(state) {
        case NEW: return "NEW";
        case READY: return "READY";
        case RUNNING: return "RUNNING";
        case WAITING: return "WAITING";
        case IO: return "IO";
        case TERMINATED: return "TERMINATED";
        default: return "UNKNOWN";
    }
}

void printProcessInfo(int pid) {
    Process& p = processes[pid];
    cout << "[Time " << currentTime << "] Process " << pid
         << " (Priority " << p.priority << ", State: " << stateToString(p.state) << ")" << endl;
}

// ==================== DEADLOCK DETECTION ====================

bool detectDeadlock(vector<int>& deadlockedProcesses) {
    // Simple deadlock detection: check for circular wait
    // A process is in deadlock if it's waiting for a resource and can't proceed

    set<int> inWaiting;
    queue<int> tempQueue = waitingQueue;

    while (!tempQueue.empty()) {
        inWaiting.insert(tempQueue.front());
        tempQueue.pop();
    }

    if (inWaiting.empty()) return false;

    // Check if processes in waiting queue can be satisfied
    map<int, ResourceInfo> tempAvailable = resources;
    bool progress = true;

    while (progress && !inWaiting.empty()) {
        progress = false;
        set<int> toRemove;

        for (int pid : inWaiting) {
            Process& p = processes[pid];
            if (p.waitingForResource != -1) {
                if (tempAvailable[p.waitingForResource].availableInstances >= p.waitingForAmount) {
                    // This process can be satisfied
                    toRemove.insert(pid);
                    progress = true;

                    // Release resources this process holds
                    for (auto& res : p.heldResources) {
                        tempAvailable[res.first].availableInstances += res.second;
                    }
                }
            }
        }

        for (int pid : toRemove) {
            inWaiting.erase(pid);
        }
    }

    // Remaining processes are deadlocked
    if (!inWaiting.empty()) {
        deadlockedProcesses.assign(inWaiting.begin(), inWaiting.end());
        return true;
    }

    return false;
}

void recoverFromDeadlock(vector<int>& deadlockedProcesses) {
    cout << "\n========== DEADLOCK DETECTED at Time " << currentTime << " ==========\n";
    cout << "Deadlocked processes: ";
    for (int pid : deadlockedProcesses) {
        cout << "P" << pid << " ";
    }
    cout << endl;

    // Recovery: Terminate the process with lowest priority (highest priority number)
    int victimPID = deadlockedProcesses[0];
    int lowestPriority = processes[victimPID].priority;

    for (int pid : deadlockedProcesses) {
        if (processes[pid].priority > lowestPriority) {
            lowestPriority = processes[pid].priority;
            victimPID = pid;
        }
    }

    cout << "RECOVERY: Terminating Process P" << victimPID << " (Priority " << lowestPriority << ")\n";

    // Release all resources held by victim
    Process& victim = processes[victimPID];
    for (auto& res : victim.heldResources) {
        resources[res.first].availableInstances += res.second;
        cout << "Released " << res.second << " instances of Resource R" << res.first << endl;
    }
    victim.heldResources.clear();

    // Terminate the victim process
    victim.state = TERMINATED;
    victim.completionTime = currentTime;

    // Remove from waiting queue
    queue<int> newWaitingQueue;
    queue<int> temp = waitingQueue;
    while (!temp.empty()) {
        int pid = temp.front();
        temp.pop();
        if (pid != victimPID) {
            newWaitingQueue.push(pid);
        }
    }
    waitingQueue = newWaitingQueue;

    cout << "========== DEADLOCK RECOVERY COMPLETE ==========\n\n";
}

// ==================== RESOURCE MANAGEMENT ====================

bool requestResource(int pid, int resourceID, int amount) {
    Process& p = processes[pid];

    if (resources[resourceID].availableInstances >= amount) {
        resources[resourceID].availableInstances -= amount;
        p.heldResources[resourceID] += amount;
        cout << "[Time " << currentTime << "] P" << pid << " acquired "
             << amount << " instances of R" << resourceID << endl;
        return true;
    } else {
        cout << "[Time " << currentTime << "] P" << pid << " waiting for "
             << amount << " instances of R" << resourceID
             << " (Available: " << resources[resourceID].availableInstances << ")" << endl;
        p.waitingForResource = resourceID;
        p.waitingForAmount = amount;
        return false;
    }
}

void releaseResource(int pid, int resourceID, int amount) {
    Process& p = processes[pid];

    if (p.heldResources[resourceID] >= amount) {
        p.heldResources[resourceID] -= amount;
        resources[resourceID].availableInstances += amount;
        cout << "[Time " << currentTime << "] P" << pid << " released "
             << amount << " instances of R" << resourceID << endl;
    }
}

// ==================== SCHEDULING FUNCTIONS ====================

void applyAging() {
    queue<int> tempQueue;

    while (!readyQueue.empty()) {
        int pid = readyQueue.front();
        readyQueue.pop();

        Process& p = processes[pid];
        p.timeInReadyQueue++;

        if (p.timeInReadyQueue >= 10 && p.priority > 0) {
            p.priority--;
            p.timeInReadyQueue = 0;
            cout << "[Time " << currentTime << "] AGING: P" << pid
                 << " priority decreased to " << p.priority << endl;
        }

        tempQueue.push(pid);
    }

    readyQueue = tempQueue;
}

int selectNextProcess() {
    if (readyQueue.empty()) return -1;

    // Find process with highest priority (lowest priority number)
    int bestPID = -1;
    int bestPriority = 100;
    queue<int> tempQueue;

    while (!readyQueue.empty()) {
        int pid = readyQueue.front();
        readyQueue.pop();
        tempQueue.push(pid);

        if (processes[pid].priority < bestPriority) {
            bestPriority = processes[pid].priority;
            bestPID = pid;
        }
    }

    // Remove selected process from queue
    readyQueue = queue<int>();
    while (!tempQueue.empty()) {
        int pid = tempQueue.front();
        tempQueue.pop();
        if (pid != bestPID) {
            readyQueue.push(pid);
        }
    }

    return bestPID;
}

void executeProcess() {
    if (currentProcess == -1) return;

    Process& p = processes[currentProcess];

    if (p.currentBurstIndex >= p.bursts.size()) {
        // Process finished
        p.state = TERMINATED;
        p.completionTime = currentTime;

        // Release all held resources
        for (auto& res : p.heldResources) {
            resources[res.first].availableInstances += res.second;
        }
        p.heldResources.clear();

        cout << "[Time " << currentTime << "] P" << p.pid << " TERMINATED\n";
        currentProcess = -1;
        return;
    }

    if (!p.isCPUBurst[p.currentBurstIndex]) {
        // This shouldn't happen - IO bursts should be in IO queue
        return;
    }

    // Execute current operation
    vector<Operation>& currentBurst = p.bursts[p.currentBurstIndex];

    if (p.currentOperationIndex >= currentBurst.size()) {
        // Current CPU burst finished
        p.currentBurstIndex++;
        p.currentOperationIndex = 0;
        p.remainingTime = 0;

        if (p.currentBurstIndex < p.bursts.size() && !p.isCPUBurst[p.currentBurstIndex]) {
            // Move to IO
            p.state = IO;
            ioQueue.push(p.pid);

            // Set remaining time for IO burst
            if (!p.bursts[p.currentBurstIndex].empty()) {
                p.remainingTime = p.bursts[p.currentBurstIndex][0].duration;
            }

            cout << "[Time " << currentTime << "] P" << p.pid << " moved to IO (duration "
                 << p.remainingTime << ")\n";
            currentProcess = -1;
        } else if (p.currentBurstIndex >= p.bursts.size()) {
            // Process finished
            p.state = TERMINATED;
            p.completionTime = currentTime;

            // Release all held resources
            for (auto& res : p.heldResources) {
                resources[res.first].availableInstances += res.second;
            }
            p.heldResources.clear();

            cout << "[Time " << currentTime << "] P" << p.pid << " TERMINATED\n";
            currentProcess = -1;
        }
        return;
    }

    Operation& op = currentBurst[p.currentOperationIndex];

    if (op.type == REQUEST) {
        if (requestResource(p.pid, op.resourceID, op.amount)) {
            p.currentOperationIndex++;
        } else {
            // Move to waiting queue
            p.state = WAITING;
            waitingQueue.push(p.pid);
            currentProcess = -1;
        }
    } else if (op.type == RELEASE) {
        releaseResource(p.pid, op.resourceID, op.amount);
        p.currentOperationIndex++;
    } else if (op.type == EXEC) {
        if (p.remainingTime == 0) {
            p.remainingTime = op.duration;
        }

        p.remainingTime--;

        if (p.remainingTime == 0) {
            p.currentOperationIndex++;
        }
    }
}

void processIO() {
    queue<int> tempQueue;

    while (!ioQueue.empty()) {
        int pid = ioQueue.front();
        ioQueue.pop();

        Process& p = processes[pid];
        p.remainingTime--;

        if (p.remainingTime == 0) {
            // IO finished, move to next burst
            p.currentBurstIndex++;
            p.currentOperationIndex = 0;

            if (p.currentBurstIndex < p.bursts.size()) {
                // Move back to ready queue
                p.state = READY;
                readyQueue.push(pid);
                cout << "[Time " << currentTime << "] P" << pid << " IO completed, moved to READY\n";
            }
        } else {
            tempQueue.push(pid);
        }
    }

    ioQueue = tempQueue;
}

void processWaiting() {
    queue<int> tempQueue;

    while (!waitingQueue.empty()) {
        int pid = waitingQueue.front();
        waitingQueue.pop();

        Process& p = processes[pid];

        if (p.waitingForResource != -1) {
            if (resources[p.waitingForResource].availableInstances >= p.waitingForAmount) {
                // Resource now available
                resources[p.waitingForResource].availableInstances -= p.waitingForAmount;
                p.heldResources[p.waitingForResource] += p.waitingForAmount;

                cout << "[Time " << currentTime << "] P" << pid << " acquired waited resource R"
                     << p.waitingForResource << endl;

                p.waitingForResource = -1;
                p.waitingForAmount = 0;
                p.currentOperationIndex++;

                // Move back to ready queue
                p.state = READY;
                readyQueue.push(pid);
            } else {
                tempQueue.push(pid);
            }
        } else {
            tempQueue.push(pid);
        }
    }

    waitingQueue = tempQueue;
}

void checkNewArrivals() {
    for (auto& p : processes) {
        if (p.state == NEW && p.arrivalTime <= currentTime) {
            p.state = READY;
            readyQueue.push(p.pid);
            cout << "[Time " << currentTime << "] P" << p.pid << " arrived (Priority "
                 << p.priority << ")\n";
        }
    }
}

// ==================== INPUT PARSING ====================

void parseInput(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Cannot open file " << filename << endl;
        exit(1);
    }

    string line;

    // Parse resources (first line)
    getline(file, line);
    stringstream ss(line);
    char ch;

    while (ss >> ch) {
        if (ch == '[') {
            int id, instances;
            ss >> id >> ch >> instances >> ch;  // Read [id,instances]
            resources[id] = ResourceInfo(instances);
        }
    }

    // Parse processes
    while (getline(file, line)) {
        if (line.empty()) continue;

        Process p;
        stringstream lineStream(line);

        lineStream >> p.pid >> p.arrivalTime >> p.priority;
        p.originalPriority = p.priority;

        // Parse bursts
        string token;
        while (lineStream >> token) {
            if (token.find("CPU") == 0) {
                // CPU burst
                size_t start = token.find('{');
                size_t end = token.find('}');
                string burstContent = token.substr(start + 1, end - start - 1);

                vector<Operation> ops;

                // Split by comma, but respect brackets
                vector<string> items;
                string item;
                int bracketDepth = 0;

                for (char c : burstContent) {
                    if (c == '[') {
                        bracketDepth++;
                        item += c;
                    } else if (c == ']') {
                        bracketDepth--;
                        item += c;
                    } else if (c == ',' && bracketDepth == 0) {
                        items.push_back(item);
                        item.clear();
                    } else if (c != ' ' || !item.empty()) {
                        item += c;
                    }
                }
                if (!item.empty()) items.push_back(item);

                // Process each item
                for (string& item : items) {
                    // Trim whitespace
                    item.erase(0, item.find_first_not_of(" \t"));
                    item.erase(item.find_last_not_of(" \t") + 1);

                    if (item.empty()) continue;

                    if (item[0] == 'R') {
                        // Request: R[id,amount]
                        size_t s = item.find('[');
                        size_t e = item.find(']');
                        string params = item.substr(s + 1, e - s - 1);
                        stringstream paramStream(params);
                        int id = 0, amt = 0;
                        char comma;
                        paramStream >> id >> comma >> amt;
                        ops.push_back(Operation(REQUEST, id, amt));
                    } else if (item[0] == 'F') {
                        // Release: F[id,amount]
                        size_t s = item.find('[');
                        size_t e = item.find(']');
                        string params = item.substr(s + 1, e - s - 1);
                        stringstream paramStream(params);
                        int id = 0, amt = 0;
                        char comma;
                        paramStream >> id >> comma >> amt;
                        ops.push_back(Operation(RELEASE, id, amt));
                    } else {
                        // Execution time
                        int duration = stoi(item);
                        ops.push_back(Operation(EXEC, duration));
                    }
                }

                p.bursts.push_back(ops);
                p.isCPUBurst.push_back(true);

            } else if (token.find("IO") == 0) {
                // IO burst
                size_t start = token.find('{');
                size_t end = token.find('}');
                string duration = token.substr(start + 1, end - start - 1);

                vector<Operation> ops;
                ops.push_back(Operation(EXEC, stoi(duration)));

                p.bursts.push_back(ops);
                p.isCPUBurst.push_back(false);
            }
        }

        processes.push_back(p);
    }

    file.close();
}

// ==================== OUTPUT FUNCTIONS ====================

void printGanttChart() {
    cout << "\n========== GANTT CHART ==========\n";

    if (ganttChart.empty()) {
        cout << "No processes executed.\n";
        return;
    }

    cout << "|";
    for (const auto& entry : ganttChart) {
        cout << " P" << entry.pid << " |";
    }
    cout << "\n";

    cout << currentTime;
    for (const auto& entry : ganttChart) {
        int width = to_string(entry.pid).length() + 3;
        cout << setw(width) << entry.endTime;
    }
    cout << "\n";
}

void printStatistics() {
    cout << "\n========== STATISTICS ==========\n";

    double totalWaitingTime = 0;
    double totalTurnaroundTime = 0;
    int completedProcesses = 0;

    for (auto& p : processes) {
        if (p.state == TERMINATED && p.completionTime > 0) {
            p.turnaroundTime = p.completionTime - p.arrivalTime;
            p.waitingTime = p.turnaroundTime;

            // Calculate actual CPU time used
            int cpuTime = 0;
            for (size_t i = 0; i < p.bursts.size(); i++) {
                if (p.isCPUBurst[i]) {
                    for (const auto& op : p.bursts[i]) {
                        if (op.type == EXEC) {
                            cpuTime += op.duration;
                        }
                    }
                }
            }

            p.waitingTime -= cpuTime;

            totalWaitingTime += p.waitingTime;
            totalTurnaroundTime += p.turnaroundTime;
            completedProcesses++;

            cout << "Process P" << p.pid << ":\n";
            cout << "  Arrival Time: " << p.arrivalTime << "\n";
            cout << "  Completion Time: " << p.completionTime << "\n";
            cout << "  Turnaround Time: " << p.turnaroundTime << "\n";
            cout << "  Waiting Time: " << p.waitingTime << "\n";
        }
    }

    if (completedProcesses > 0) {
        cout << "\nAverage Waiting Time: " << (totalWaitingTime / completedProcesses) << "\n";
        cout << "Average Turnaround Time: " << (totalTurnaroundTime / completedProcesses) << "\n";
    }
}

// ==================== MAIN SIMULATION ====================

void simulate() {
    cout << "\n========== SIMULATION START ==========\n\n";

    int timeSlice = 0;
    int ganttStartTime = 0;
    int lastProcess = -1;

    while (true) {
        // Check for new arrivals
        checkNewArrivals();

        // Process IO operations
        processIO();

        // Process waiting queue
        processWaiting();

        // Deadlock detection (every 5 time units)
        if (currentTime % 5 == 0 && currentTime > 0) {
            vector<int> deadlocked;
            if (detectDeadlock(deadlocked)) {
                recoverFromDeadlock(deadlocked);
                processWaiting();  // Try to unblock processes after recovery
            }
        }

        // Apply aging
        if (currentTime % 1 == 0 && !readyQueue.empty()) {
            applyAging();
        }

        // Select next process if no current process
        if (currentProcess == -1) {
            currentProcess = selectNextProcess();
            timeSlice = 0;

            if (currentProcess != -1) {
                processes[currentProcess].state = RUNNING;
                if (processes[currentProcess].startTime == -1) {
                    processes[currentProcess].startTime = currentTime;
                }
                cout << "[Time " << currentTime << "] P" << currentProcess << " started/resumed\n";
            }
        }

        // Execute current process
        if (currentProcess != -1) {
            if (lastProcess != currentProcess) {
                if (lastProcess != -1) {
                    ganttChart.push_back({lastProcess, ganttStartTime, currentTime});
                }
                ganttStartTime = currentTime;
                lastProcess = currentProcess;
            }

            executeProcess();
            timeSlice++;

            // Round-robin time quantum preemption
            if (currentProcess != -1 && timeSlice >= TIME_QUANTUM) {
                // Check if there are other processes with same priority
                bool shouldPreempt = false;
                queue<int> tempQueue = readyQueue;
                while (!tempQueue.empty()) {
                    int pid = tempQueue.front();
                    tempQueue.pop();
                    if (processes[pid].priority <= processes[currentProcess].priority) {
                        shouldPreempt = true;
                        break;
                    }
                }

                if (shouldPreempt && processes[currentProcess].state == RUNNING) {
                    cout << "[Time " << currentTime << "] Time quantum expired for P"
                         << currentProcess << ", preempting\n";
                    processes[currentProcess].state = READY;
                    readyQueue.push(currentProcess);
                    currentProcess = -1;
                    timeSlice = 0;
                }
            }
        }

        // Check if all processes are done
        bool allDone = true;
        for (const auto& p : processes) {
            if (p.state != TERMINATED) {
                allDone = false;
                break;
            }
        }

        if (allDone) {
            if (lastProcess != -1) {
                ganttChart.push_back({lastProcess, ganttStartTime, currentTime});
            }
            break;
        }

        // Check for infinite loop (no progress)
        if (currentProcess == -1 && readyQueue.empty() &&
            ioQueue.empty() && !waitingQueue.empty()) {
            // Deadlock not resolved
            vector<int> deadlocked;
            if (detectDeadlock(deadlocked)) {
                recoverFromDeadlock(deadlocked);
            } else {
                cout << "[Time " << currentTime << "] Warning: System may be stuck\n";
                break;
            }
        }

        currentTime++;

        if (currentTime > 10000) {
            cout << "Simulation timeout at time 10000\n";
            break;
        }
    }

    cout << "\n========== SIMULATION END ==========\n";
}

// ==================== MAIN ====================

int main(int argc, char* argv[]) {
    string filename = "inputFile.txt";

    if (argc > 1) {
        filename = argv[1];
    }

    cout << "CPU Scheduling Simulator with Deadlock Detection\n";
    cout << "================================================\n";
    cout << "Reading input from: " << filename << "\n";

    parseInput(filename);

    cout << "\nResources:\n";
    for (const auto& r : resources) {
        cout << "  R" << r.first << ": " << r.second.totalInstances << " instances\n";
    }

    cout << "\nProcesses: " << processes.size() << "\n";
    for (const auto& p : processes) {
        cout << "  P" << p.pid << " arrives at " << p.arrivalTime
             << " with priority " << p.priority << "\n";
    }

    simulate();
    printGanttChart();
    printStatistics();

    return 0;
}
