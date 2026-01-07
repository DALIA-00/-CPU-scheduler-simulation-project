# CPU Scheduler Simulation with Deadlock Detection

C++ implementations of a CPU scheduling system with deadlock detection and recovery for Operating Systems course (ENCS3390).

## Three Versions Available

### 1. **student_scheduler.cpp** (⭐ MOST RECOMMENDED FOR BEGINNERS)
- ✅ **No enums** - Uses simple const strings instead
- ✅ **No size_t** - Uses int for all sizes
- ✅ **No to_string** - Custom intToStr() function
- ✅ **Very basic C++** - Only fundamental features
- ✅ **Human-readable** - Easy variable names and logic
- ✅ **Single file** - All code in one place

### 2. **simple_scheduler.cpp** (RECOMMENDED FOR STUDENTS)
- ✅ **No OOP** - Uses simple structs and functions only
- ✅ **Single file** - All code in one .cpp file
- ✅ **Easy to understand** - Procedural programming style
- ✅ **Well commented** - Clear explanation of each section

### 3. **scheduler_simulation.cpp** (Advanced)
- Uses classes and OOP principles
- More modular structure
- Better for larger projects

## Features

- **Preemptive Priority Scheduling with Round Robin**
- **Aging Mechanism**: Priority decreases by 1 every 10 time units in ready queue
- **Deadlock Detection**: Periodic checking for circular wait conditions
- **Deadlock Recovery**: Terminates lowest priority process in deadlock
- **Resource Management**: Multiple resource types with multiple instances
- **I/O Simulation**: Concurrent I/O processing
- **Gantt Chart**: Visual timeline of process execution
- **Statistics**: Average waiting time and turnaround time

## Compilation

**Student version (most beginner-friendly):**
```bash
g++ -o student_scheduler student_scheduler.cpp -std=c++11
```

**Simple version (recommended for students):**
```bash
g++ -o simple_scheduler simple_scheduler.cpp -std=c++11
```

**Advanced version:**
```bash
g++ -o scheduler scheduler_simulation.cpp -std=c++11
```

## Usage

**Student version (most beginner-friendly):**
```bash
./student_scheduler test_simple.txt
./student_scheduler input_no_deadlock.txt
./student_scheduler input_with_deadlock.txt
```

**Simple version:**
```bash
./simple_scheduler test_simple.txt
./simple_scheduler input_no_deadlock.txt
./simple_scheduler input_with_deadlock.txt
```

**Advanced version:**
```bash
./scheduler input_no_deadlock.txt
./scheduler input_with_deadlock.txt
```

## Input File Format

**First line** - System resources:
```
[ResourceID, Instances], [ResourceID, Instances], ...
```

**Subsequent lines** - Process definitions:
```
[PID] [Arrival Time] [Priority] [Sequence of CPU and IO bursts]
```

### Example

```
[1,5], [2,3], [3,2]
0 0 1 CPU{R[1,2],30,F[1,2],10}
1 2 2 CPU{20} IO{15} CPU{R[2,1],25,F[2,1],10}
```

**Explanation:**
- Resources: R1 has 5 instances, R2 has 3, R3 has 2
- Process 0: PID=0, arrives at time 0, priority 1
  - CPU burst: Request 2 instances of R1, execute 30 time units, release 2 instances of R1, execute 10 time units
- Process 1: PID=1, arrives at time 2, priority 2
  - CPU burst: execute 20 time units
  - IO burst: 15 time units
  - CPU burst: Request 1 instance of R2, execute 25 units, release 1 instance of R2, execute 10 units

### Operations

- `R[id,amount]` - Request resource (e.g., R[1,2] requests 2 instances of resource 1)
- `F[id,amount]` - Free/Release resource (e.g., F[1,2] releases 2 instances of resource 1)
- `number` - CPU execution time in time units
- `IO{duration}` - I/O burst with specified duration

## Sample Input Files

1. **input_no_deadlock.txt**: Normal execution without deadlock
2. **input_with_deadlock.txt**: Scenario designed to cause deadlock

## Key Components

### Process States
- **NEW**: Process has not arrived yet
- **READY**: Process in ready queue waiting for CPU
- **RUNNING**: Process currently executing
- **WAITING**: Process waiting for resource
- **IO**: Process performing I/O operation
- **TERMINATED**: Process completed

### Scheduling Algorithm
1. Processes are scheduled based on priority (0 is highest priority)
2. Round-robin with time quantum of 10 units for same priority
3. Preemptive - higher priority process preempts lower priority

### Aging
- Every time unit, processes in ready queue increment their wait counter
- After 10 time units, priority decreases by 1 (if priority > 0)
- Prevents starvation of low-priority processes

### Deadlock Detection
- Runs every 5 time units
- Detects circular wait conditions
- Uses resource allocation graph analysis

### Deadlock Recovery
- Selects victim: process with lowest priority (highest priority number)
- Terminates victim process
- Releases all resources held by victim
- Other processes can now proceed

## Output

The program outputs:
1. **Simulation log**: Detailed trace of events
2. **Gantt chart**: Timeline showing which process runs when
3. **Statistics**: For each process and overall averages
   - Arrival time
   - Completion time
   - Turnaround time
   - Waiting time
   - Average waiting time
   - Average turnaround time

## Example Output

```
========== GANTT CHART ==========
| P0 | P1 | P0 | P2 | P1 |
0   10   20   30   40   50

========== STATISTICS ==========
Process P0:
  Arrival Time: 0
  Completion Time: 45
  Turnaround Time: 45
  Waiting Time: 15

Average Waiting Time: 12.5
Average Turnaround Time: 42.3
```

## Testing

### Test without deadlock:
```bash
./scheduler input_no_deadlock.txt
```
Expected: All processes complete successfully, no deadlock detected

### Test with deadlock:
```bash
./scheduler input_with_deadlock.txt
```
Expected: Deadlock detected and recovered via process termination

## Implementation Notes

- **Time quantum**: 10 time units for round-robin
- **Aging threshold**: 10 time units in ready queue
- **Priority range**: 0 (highest) to 20 (lowest)
- **Context switching**: Assumed to be negligible (0 time units)
- **Deadlock check frequency**: Every 5 time units

## Project Structure

```
scheduler_simulation.cpp  - Main implementation
input_no_deadlock.txt    - Sample input without deadlock
input_with_deadlock.txt  - Sample input with deadlock
IMPLEMENTATION_README.md - This file
```

## Author Notes

This implementation is designed for educational purposes to demonstrate:
- CPU scheduling algorithms
- Resource allocation and deadlock
- Process synchronization concepts
- Operating system simulation

The code is well-commented and structured for easy understanding by students.
