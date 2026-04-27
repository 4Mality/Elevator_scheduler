# Elevator_scheduler

## Overview

This repository contains a simple elevator scheduler program in C. It reads elevator definitions and person requests from two input files and assigns each person to the best available elevator.

## Input format

### Building file
Each elevator line should contain:

- `id`
- `lowest` floor
- `highest` floor
- `current` floor
- `capacity`

Example:

```
E1 1 10 1 8
E2 1 5 3 6
```

### People file
Each person line should contain:

- `id`
- `startFloor`
- `endFloor`
- `startTime`

Example:

```
P1 2 8 0
P2 5 1 3
```

## Build and run

Compile the program:

```bash
gcc scheduler_os -o scheduler
```

Run the scheduler:

```bash
./scheduler building.txt people.txt
```

## Behavior

The program sorts people by `startTime`, then assigns each person to the elevator that can deliver them the earliest. It prints pickup and dropoff times for each assignment.

If a person cannot be assigned because no elevator serves both floors, the program reports that the person could not be assigned.
