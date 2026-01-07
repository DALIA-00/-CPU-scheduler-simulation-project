/*
==============================================================================
  CPU SCHEDULER SIMULATION - STUDENT VERSION

  A simple implementation for Operating Systems course
  No advanced features - just basic C++ for easy understanding

  Features:
  - Priority Scheduling with Round Robin
  - Aging to prevent starvation
  - Deadlock Detection and Recovery
  - Resource Management
  - I/O Simulation
==============================================================================
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>

using namespace std;

// ==================== SIMPLE CONSTANTS ====================

// Process states (using simple strings)
const string STATE_NEW = "NEW";
const string STATE_READY = "READY";
const string STATE_RUNNING = "RUNNING";
const string STATE_WAITING = "WAITING";
const string STATE_IO = "IO";
const string STATE_TERMINATED = "TERMINATED";

// Operation types (using simple strings)
const string OP_EXEC = "EXEC";
const string OP_REQUEST = "REQUEST";
const string OP_RELEASE = "RELEASE";

// Simulation parameters
const int TIME_QUANTUM = 10;        // Round robin time slice
const int AGING_THRESHOLD = 10;     // Time units before priority boost
const int DEADLOCK_CHECK = 5;       // Check deadlock every 5 time units

// ==================== DATA STRUCTURES ====================

// Represents a single operation within a CPU burst
struct Operation {
    string type;           // "EXEC", "REQUEST", or "RELEASE"
    int resourceID;        // Which resource (for REQUEST/RELEASE)
    int amount;            // How many instances
    int duration;          // How long to execute (for EXEC)
};

// Represents a CPU or IO burst
struct Burst {
    bool isCPU;                      // true = CPU burst, false = IO burst
    vector<Operation> operations;    // List of operations in this burst
};

// Represents a process
struct Process {
    // Basic info
    int pid;                    // Process ID
    int arrivalTime;            // When process arrives
    int priority;               // Current priority (0 = highest)
    int originalPriority;       // Starting priority
    string state;               // Current state

    // Burst information
    vector<Burst> bursts;       // All CPU and IO bursts
    int currentBurstIndex;      // Which burst we're on
    int currentOpIndex;         // Which operation in current burst
    int remainingTime;          // Time left for current operation

    // Scheduling information
    int timeInQueue;            // Time spent in ready queue (for aging)
    int startTime;              // When first executed (-1 if not started)
    int finishTime;             // When completed

    // Resource tracking
    map<int, int> heldResources;     // resourceID -> amount held
    int waitingResource;             // Which resource waiting for (-1 if none)
    int waitingAmount;               // How many instances needed
};

// Represents a system resource
struct Resource {
    int total;          // Total instances
    int available;      // Currently available
};

// For Gantt chart
struct GanttEntry {
    int pid;
    int start;
    int end;
};

// ==================== GLOBAL VARIABLES ====================

map<int, Resource> resources;           // All system resources
vector<Process> processes;              // All processes
vector<int> readyQueue;                 // Processes ready to run
vector<int> ioQueue;                    // Processes doing I/O
vector<int> waitingQueue;               // Processes waiting for resources
int currentProcess = -1;                // Currently running process (-1 = none)
int currentTime = 0;                    // Current simulation time
vector<GanttEntry> ganttChart;          // For visualization
int timeSlice = 0;                      // Current time quantum counter

// ==================== HELPER FUNCTIONS ====================

// Convert integer to string (simple version without intToStr)
string intToStr(int num) {
    if (num == 0) return "0";

    string result = "";
    bool negative = false;

    if (num < 0) {
        negative = true;
        num = -num;
    }

    while (num > 0) {
        result = char('0' + (num % 10)) + result;
        num /= 10;
    }

    if (negative) {
        result = "-" + result;
    }

    return result;
}

// Print current time and process info
void printTime(string message) {
    cout << "[Time " << currentTime << "] " << message << endl;
}

// Check if a process can get the resources it needs
bool canGetResource(int pid, map<int, Resource>& tempResources) {
    Process& p = processes[pid];
    if (p.waitingResource == -1) {
        return true;  // Not waiting for anything
    }
    return tempResources[p.waitingResource].available >= p.waitingAmount;
}

// ==================== DEADLOCK DETECTION ====================

// Check if there's a deadlock among waiting processes
bool findDeadlock(vector<int>& deadlockedProcesses) {
    if (waitingQueue.empty()) {
        return false;  // No waiting processes = no deadlock
    }

    // Make copy of resources to simulate
    map<int, Resource> tempResources = resources;
    vector<int> stillWaiting = waitingQueue;
    bool madeProgress = true;

    // Keep trying to satisfy processes
    while (madeProgress && !stillWaiting.empty()) {
        madeProgress = false;
        vector<int> newWaiting;

        for (int i = 0; i < stillWaiting.size(); i++) {
            int pid = stillWaiting[i];

            if (canGetResource(pid, tempResources)) {
                // This process can finish, release its resources
                madeProgress = true;

                // Give back all resources it holds
                for (map<int, int>::iterator it = processes[pid].heldResources.begin();
                     it != processes[pid].heldResources.end(); it++) {
                    tempResources[it->first].available += it->second;
                }
            } else {
                // Still can't proceed
                newWaiting.push_back(pid);
            }
        }

        stillWaiting = newWaiting;
    }

    // Any processes still waiting are deadlocked
    if (!stillWaiting.empty()) {
        deadlockedProcesses = stillWaiting;
        return true;
    }

    return false;
}

// Recover from deadlock by killing a process
void breakDeadlock(vector<int>& deadlockedProcesses) {
    cout << "\n========== DEADLOCK DETECTED at Time " << currentTime << " ==========\n";
    cout << "Deadlocked processes: ";
    for (int i = 0; i < deadlockedProcesses.size(); i++) {
        cout << "P" << deadlockedProcesses[i] << " ";
    }
    cout << "\n";

    // Find victim: process with lowest priority (highest priority number)
    int victim = deadlockedProcesses[0];
    int lowestPriority = processes[victim].priority;

    for (int i = 1; i < deadlockedProcesses.size(); i++) {
        int pid = deadlockedProcesses[i];
        if (processes[pid].priority > lowestPriority) {
            lowestPriority = processes[pid].priority;
            victim = pid;
        }
    }

    cout << "RECOVERY: Killing Process P" << victim
         << " (Priority " << lowestPriority << ")\n";

    // Release all resources held by victim
    for (map<int, int>::iterator it = processes[victim].heldResources.begin();
         it != processes[victim].heldResources.end(); it++) {
        resources[it->first].available += it->second;
        cout << "Released " << it->second << " instances of Resource R"
             << it->first << "\n";
    }
    processes[victim].heldResources.clear();

    // Kill the process
    processes[victim].state = STATE_TERMINATED;
    processes[victim].finishTime = currentTime;

    // Remove from waiting queue
    vector<int> newWaiting;
    for (int i = 0; i < waitingQueue.size(); i++) {
        if (waitingQueue[i] != victim) {
            newWaiting.push_back(waitingQueue[i]);
        }
    }
    waitingQueue = newWaiting;

    cout << "========== DEADLOCK RESOLVED ==========\n\n";
}

// ==================== RESOURCE MANAGEMENT ====================

// Try to allocate resource to process
bool allocateResource(int pid, int resourceID, int amount) {
    if (resources[resourceID].available >= amount) {
        // Enough resources available
        resources[resourceID].available -= amount;
        processes[pid].heldResources[resourceID] += amount;

        cout << "[Time " << currentTime << "] P" << pid << " got "
             << amount << " instances of R" << resourceID << "\n";
        return true;
    }

    // Not enough resources
    cout << "[Time " << currentTime << "] P" << pid << " waiting for "
         << amount << " instances of R" << resourceID
         << " (only " << resources[resourceID].available << " available)\n";

    processes[pid].waitingResource = resourceID;
    processes[pid].waitingAmount = amount;
    return false;
}

// Release resource from process
void freeResource(int pid, int resourceID, int amount) {
    if (processes[pid].heldResources[resourceID] >= amount) {
        processes[pid].heldResources[resourceID] -= amount;
        resources[resourceID].available += amount;

        cout << "[Time " << currentTime << "] P" << pid << " released "
             << amount << " instances of R" << resourceID << "\n";
    }
}

// ==================== SCHEDULING FUNCTIONS ====================

// Age processes to prevent starvation
void ageProcesses() {
    for (int i = 0; i < readyQueue.size(); i++) {
        int pid = readyQueue[i];
        processes[pid].timeInQueue++;

        // After waiting 10 time units, boost priority
        if (processes[pid].timeInQueue >= AGING_THRESHOLD &&
            processes[pid].priority > 0) {
            processes[pid].priority--;
            processes[pid].timeInQueue = 0;

            cout << "[Time " << currentTime << "] AGING: P" << pid
                 << " priority boosted to " << processes[pid].priority << "\n";
        }
    }
}

// Pick next process to run (highest priority)
int pickNextProcess() {
    if (readyQueue.empty()) {
        return -1;  // No process available
    }

    // Find process with best priority (lowest number)
    int bestIndex = 0;
    int bestPriority = processes[readyQueue[0]].priority;

    for (int i = 1; i < readyQueue.size(); i++) {
        int pid = readyQueue[i];
        if (processes[pid].priority < bestPriority) {
            bestPriority = processes[pid].priority;
            bestIndex = i;
        }
    }

    // Remove from ready queue
    int selectedPID = readyQueue[bestIndex];
    readyQueue.erase(readyQueue.begin() + bestIndex);

    return selectedPID;
}

// Run current process for one time unit
void runProcess() {
    if (currentProcess == -1) {
        return;  // No process running
    }

    Process& p = processes[currentProcess];

    // Check if process is done
    if (p.currentBurstIndex >= p.bursts.size()) {
        p.state = STATE_TERMINATED;
        p.finishTime = currentTime;

        // Release all held resources
        for (map<int, int>::iterator it = p.heldResources.begin();
             it != p.heldResources.end(); it++) {
            resources[it->first].available += it->second;
        }
        p.heldResources.clear();

        printTime("P" + intToStr(p.pid) + " FINISHED");
        currentProcess = -1;
        return;
    }

    Burst& currentBurst = p.bursts[p.currentBurstIndex];

    // Should be CPU burst
    if (!currentBurst.isCPU) {
        return;
    }

    // Check if current burst is done
    if (p.currentOpIndex >= currentBurst.operations.size()) {
        // Move to next burst
        p.currentBurstIndex++;
        p.currentOpIndex = 0;
        p.remainingTime = 0;

        // Check what's next
        if (p.currentBurstIndex < p.bursts.size()) {
            if (!p.bursts[p.currentBurstIndex].isCPU) {
                // Move to I/O
                p.state = STATE_IO;
                ioQueue.push_back(p.pid);
                p.remainingTime = p.bursts[p.currentBurstIndex].operations[0].duration;

                printTime("P" + intToStr(p.pid) + " moved to I/O (duration " +
                         intToStr(p.remainingTime) + ")");
                currentProcess = -1;
            }
        } else {
            // Process finished
            p.state = STATE_TERMINATED;
            p.finishTime = currentTime;

            for (map<int, int>::iterator it = p.heldResources.begin();
                 it != p.heldResources.end(); it++) {
                resources[it->first].available += it->second;
            }
            p.heldResources.clear();

            printTime("P" + intToStr(p.pid) + " FINISHED");
            currentProcess = -1;
        }
        return;
    }

    // Execute current operation
    Operation& op = currentBurst.operations[p.currentOpIndex];

    if (op.type == OP_REQUEST) {
        // Request resource
        if (allocateResource(p.pid, op.resourceID, op.amount)) {
            p.currentOpIndex++;  // Got resource, move on
        } else {
            // Couldn't get resource, go to waiting
            p.state = STATE_WAITING;
            waitingQueue.push_back(p.pid);
            currentProcess = -1;
        }

    } else if (op.type == OP_RELEASE) {
        // Release resource
        freeResource(p.pid, op.resourceID, op.amount);
        p.currentOpIndex++;

    } else if (op.type == OP_EXEC) {
        // Execute for time
        if (p.remainingTime == 0) {
            p.remainingTime = op.duration;
        }

        p.remainingTime--;

        if (p.remainingTime == 0) {
            p.currentOpIndex++;  // Done with this operation
        }
    }
}

// Handle I/O operations
void handleIO() {
    vector<int> stillInIO;

    for (int i = 0; i < ioQueue.size(); i++) {
        int pid = ioQueue[i];
        processes[pid].remainingTime--;

        if (processes[pid].remainingTime == 0) {
            // I/O done
            processes[pid].currentBurstIndex++;
            processes[pid].currentOpIndex = 0;

            if (processes[pid].currentBurstIndex < processes[pid].bursts.size()) {
                // Move back to ready queue
                processes[pid].state = STATE_READY;
                readyQueue.push_back(pid);
                printTime("P" + intToStr(pid) + " I/O complete, back to READY");
            }
        } else {
            stillInIO.push_back(pid);
        }
    }

    ioQueue = stillInIO;
}

// Check if waiting processes can get resources now
void handleWaiting() {
    vector<int> stillWaiting;

    for (int i = 0; i < waitingQueue.size(); i++) {
        int pid = waitingQueue[i];
        Process& p = processes[pid];

        if (p.waitingResource != -1) {
            if (resources[p.waitingResource].available >= p.waitingAmount) {
                // Resource available now
                resources[p.waitingResource].available -= p.waitingAmount;
                p.heldResources[p.waitingResource] += p.waitingAmount;

                printTime("P" + intToStr(pid) + " got resource R" +
                         intToStr(p.waitingResource));

                p.waitingResource = -1;
                p.waitingAmount = 0;
                p.currentOpIndex++;

                // Move to ready queue
                p.state = STATE_READY;
                readyQueue.push_back(pid);
            } else {
                stillWaiting.push_back(pid);
            }
        } else {
            stillWaiting.push_back(pid);
        }
    }

    waitingQueue = stillWaiting;
}

// Check for newly arrived processes
void checkArrivals() {
    for (int i = 0; i < processes.size(); i++) {
        if (processes[i].state == STATE_NEW &&
            processes[i].arrivalTime <= currentTime) {

            processes[i].state = STATE_READY;
            readyQueue.push_back(i);
            printTime("P" + intToStr(i) + " arrived (Priority " +
                     intToStr(processes[i].priority) + ")");
        }
    }
}

// ==================== INPUT PARSING ====================

void loadInput(string filename) {
    ifstream file(filename.c_str());
    if (!file.is_open()) {
        cout << "ERROR: Cannot open file " << filename << "\n";
        exit(1);
    }

    string line;

    // Read resources (first line)
    getline(file, line);

    // Parse resources: [1,5], [2,3]
    for (int i = 0; i < line.length(); i++) {
        if (line[i] == '[') {
            int id = 0, count = 0;
            int j = i + 1;

            // Read ID
            while (j < line.length() && line[j] != ',') {
                id = id * 10 + (line[j] - '0');
                j++;
            }
            j++;  // Skip comma

            // Read count
            while (j < line.length() && line[j] != ']') {
                count = count * 10 + (line[j] - '0');
                j++;
            }

            Resource r;
            r.total = count;
            r.available = count;
            resources[id] = r;

            i = j;
        }
    }

    // Read processes
    while (getline(file, line)) {
        if (line.empty()) continue;

        Process p;
        p.state = STATE_NEW;
        p.currentBurstIndex = 0;
        p.currentOpIndex = 0;
        p.remainingTime = 0;
        p.timeInQueue = 0;
        p.startTime = -1;
        p.finishTime = 0;
        p.waitingResource = -1;
        p.waitingAmount = 0;

        stringstream ss(line);
        ss >> p.pid >> p.arrivalTime >> p.priority;
        p.originalPriority = p.priority;

        // Parse bursts
        string token;
        while (ss >> token) {
            if (token.find("CPU") == 0) {
                // CPU burst
                Burst burst;
                burst.isCPU = true;

                // Find content between { }
                int start = token.find('{');
                int end = token.find('}');
                string content = token.substr(start + 1, end - start - 1);

                // Split by comma (respecting brackets)
                vector<string> items;
                string item = "";
                int depth = 0;

                for (int i = 0; i < content.length(); i++) {
                    char c = content[i];

                    if (c == '[') {
                        depth++;
                        item += c;
                    } else if (c == ']') {
                        depth--;
                        item += c;
                    } else if (c == ',' && depth == 0) {
                        items.push_back(item);
                        item = "";
                    } else if (c != ' ' || !item.empty()) {
                        item += c;
                    }
                }
                if (!item.empty()) {
                    items.push_back(item);
                }

                // Parse each item
                for (int i = 0; i < items.size(); i++) {
                    string item = items[i];

                    // Trim spaces
                    while (!item.empty() && (item[0] == ' ' || item[0] == '\t')) {
                        item = item.substr(1);
                    }
                    while (!item.empty() && (item[item.length()-1] == ' ' ||
                                            item[item.length()-1] == '\t')) {
                        item = item.substr(0, item.length()-1);
                    }

                    if (item.empty()) continue;

                    Operation op;

                    if (item[0] == 'R') {
                        // Request: R[id,amount]
                        op.type = OP_REQUEST;
                        int s = item.find('[');
                        int e = item.find(']');
                        string params = item.substr(s + 1, e - s - 1);

                        // Parse id,amount
                        int commaPos = params.find(',');
                        string idStr = params.substr(0, commaPos);
                        string amtStr = params.substr(commaPos + 1);

                        op.resourceID = atoi(idStr.c_str());
                        op.amount = atoi(amtStr.c_str());
                        op.duration = 0;

                    } else if (item[0] == 'F') {
                        // Release: F[id,amount]
                        op.type = OP_RELEASE;
                        int s = item.find('[');
                        int e = item.find(']');
                        string params = item.substr(s + 1, e - s - 1);

                        int commaPos = params.find(',');
                        string idStr = params.substr(0, commaPos);
                        string amtStr = params.substr(commaPos + 1);

                        op.resourceID = atoi(idStr.c_str());
                        op.amount = atoi(amtStr.c_str());
                        op.duration = 0;

                    } else {
                        // Execute duration
                        op.type = OP_EXEC;
                        op.duration = atoi(item.c_str());
                        op.resourceID = 0;
                        op.amount = 0;
                    }

                    burst.operations.push_back(op);
                }

                p.bursts.push_back(burst);

            } else if (token.find("IO") == 0) {
                // I/O burst
                Burst burst;
                burst.isCPU = false;

                int start = token.find('{');
                int end = token.find('}');
                string duration = token.substr(start + 1, end - start - 1);

                Operation op;
                op.type = OP_EXEC;
                op.duration = atoi(duration.c_str());
                op.resourceID = 0;
                op.amount = 0;

                burst.operations.push_back(op);
                p.bursts.push_back(burst);
            }
        }

        processes.push_back(p);
    }

    file.close();
}

// ==================== OUTPUT FUNCTIONS ====================

void showGanttChart() {
    cout << "\n========== GANTT CHART ==========\n";

    if (ganttChart.empty()) {
        cout << "No processes ran.\n";
        return;
    }

    cout << "|";
    for (int i = 0; i < ganttChart.size(); i++) {
        cout << " P" << ganttChart[i].pid << " |";
    }
    cout << "\n";

    cout << "0";
    for (int i = 0; i < ganttChart.size(); i++) {
        cout << "    " << ganttChart[i].end;
    }
    cout << "\n";
}

void showStats() {
    cout << "\n========== STATISTICS ==========\n";

    double totalWaiting = 0;
    double totalTurnaround = 0;
    int completed = 0;

    for (int i = 0; i < processes.size(); i++) {
        Process& p = processes[i];

        if (p.state == STATE_TERMINATED && p.finishTime > 0) {
            int turnaround = p.finishTime - p.arrivalTime;

            // Calculate CPU time
            int cpuTime = 0;
            for (int j = 0; j < p.bursts.size(); j++) {
                if (p.bursts[j].isCPU) {
                    for (int k = 0; k < p.bursts[j].operations.size(); k++) {
                        if (p.bursts[j].operations[k].type == OP_EXEC) {
                            cpuTime += p.bursts[j].operations[k].duration;
                        }
                    }
                }
            }

            int waiting = turnaround - cpuTime;

            totalWaiting += waiting;
            totalTurnaround += turnaround;
            completed++;

            cout << "Process P" << p.pid << ":\n";
            cout << "  Arrival: " << p.arrivalTime << "\n";
            cout << "  Finish: " << p.finishTime << "\n";
            cout << "  Turnaround: " << turnaround << "\n";
            cout << "  Waiting: " << waiting << "\n";
        }
    }

    if (completed > 0) {
        cout << "\nAverage Waiting Time: " << (totalWaiting / completed) << "\n";
        cout << "Average Turnaround Time: " << (totalTurnaround / completed) << "\n";
    }
}

// ==================== MAIN SIMULATION ====================

void runSimulation() {
    cout << "\n========== SIMULATION START ==========\n\n";

    int lastPID = -1;
    int ganttStart = 0;

    while (true) {
        // 1. Check for new arrivals
        checkArrivals();

        // 2. Handle I/O
        handleIO();

        // 3. Check waiting processes
        handleWaiting();

        // 4. Deadlock detection (every 5 time units)
        if (currentTime % DEADLOCK_CHECK == 0 && currentTime > 0) {
            vector<int> deadlocked;
            if (findDeadlock(deadlocked)) {
                breakDeadlock(deadlocked);
                handleWaiting();  // Try again after recovery
            }
        }

        // 5. Apply aging
        if (!readyQueue.empty()) {
            ageProcesses();
        }

        // 6. Select process if none running
        if (currentProcess == -1) {
            currentProcess = pickNextProcess();
            timeSlice = 0;

            if (currentProcess != -1) {
                processes[currentProcess].state = STATE_RUNNING;
                if (processes[currentProcess].startTime == -1) {
                    processes[currentProcess].startTime = currentTime;
                }
                printTime("P" + intToStr(currentProcess) + " running");
            }
        }

        // 7. Execute current process
        if (currentProcess != -1) {
            // Update Gantt chart
            if (lastPID != currentProcess) {
                if (lastPID != -1) {
                    GanttEntry entry;
                    entry.pid = lastPID;
                    entry.start = ganttStart;
                    entry.end = currentTime;
                    ganttChart.push_back(entry);
                }
                ganttStart = currentTime;
                lastPID = currentProcess;
            }

            runProcess();
            timeSlice++;

            // 8. Round-robin preemption
            if (currentProcess != -1 && timeSlice >= TIME_QUANTUM) {
                // Check if other processes with same or better priority
                bool shouldPreempt = false;
                for (int i = 0; i < readyQueue.size(); i++) {
                    if (processes[readyQueue[i]].priority <=
                        processes[currentProcess].priority) {
                        shouldPreempt = true;
                        break;
                    }
                }

                if (shouldPreempt && processes[currentProcess].state == STATE_RUNNING) {
                    printTime("Time quantum expired for P" + intToStr(currentProcess));
                    processes[currentProcess].state = STATE_READY;
                    readyQueue.push_back(currentProcess);
                    currentProcess = -1;
                    timeSlice = 0;
                }
            }
        }

        // 9. Check if all done
        bool allDone = true;
        for (int i = 0; i < processes.size(); i++) {
            if (processes[i].state != STATE_TERMINATED) {
                allDone = false;
                break;
            }
        }

        if (allDone) {
            if (lastPID != -1) {
                GanttEntry entry;
                entry.pid = lastPID;
                entry.start = ganttStart;
                entry.end = currentTime;
                ganttChart.push_back(entry);
            }
            break;
        }

        // Move time forward
        currentTime++;

        // Safety check
        if (currentTime > 10000) {
            cout << "Simulation timeout at 10000 time units\n";
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

    cout << "==================================================\n";
    cout << "  CPU SCHEDULER SIMULATION - STUDENT VERSION\n";
    cout << "  Simple implementation without advanced C++ features\n";
    cout << "==================================================\n";
    cout << "Reading: " << filename << "\n";

    // Load input
    loadInput(filename);

    // Show resources
    cout << "\nResources:\n";
    for (map<int, Resource>::iterator it = resources.begin();
         it != resources.end(); it++) {
        cout << "  R" << it->first << ": " << it->second.total << " instances\n";
    }

    // Show processes
    cout << "\nProcesses: " << processes.size() << "\n";
    for (int i = 0; i < processes.size(); i++) {
        cout << "  P" << processes[i].pid << " arrives at "
             << processes[i].arrivalTime << " with priority "
             << processes[i].priority << "\n";
    }

    // Run simulation
    runSimulation();
    showGanttChart();
    showStats();

    return 0;
}
