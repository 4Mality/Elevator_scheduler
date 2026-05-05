#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <pthread.h>
#include "api.h"

#define MAX_ELEVATORS 100
#define MAX_NAME      100
#define MAX_ID        128
#define MAX_RESPONSE  1024
#define QUEUE_SIZE    256

typedef struct {
    char id[MAX_NAME];
    int  lowest, highest, current, capacity;
} Elevator;

typedef struct {
    char personID[MAX_ID];
    int  startFloor, endFloor;
} PersonRequest;

typedef struct {
    char personID[MAX_ID];
    char elevatorID[MAX_NAME];
} Assignment;

static Elevator elevators[MAX_ELEVATORS];
static int      elevatorCount = 0;
static volatile int simDone   = 0;

/* input queue: input thread -> scheduler thread */
static PersonRequest inputQueue[QUEUE_SIZE];
static int inputHead = 0, inputTail = 0, inputCount = 0;
static pthread_mutex_t inputMutex    = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  inputNotEmpty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  inputNotFull  = PTHREAD_COND_INITIALIZER;

/* output queue: scheduler thread -> output thread */
static Assignment outputQueue[QUEUE_SIZE];
static int outputHead = 0, outputTail = 0, outputCount = 0;
static pthread_mutex_t outputMutex    = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  outputNotEmpty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  outputNotFull  = PTHREAD_COND_INITIALIZER;

/* pick elevator whose range covers both floors; skips full elevators, nearest wins */
static const char *pick_elevator(int startFloor, int endFloor) {
    char status[MAX_RESPONSE];
    int bestIdx       = 0;
    int bestDist      = INT_MAX;
    int foundCapacity = 0;

    for (int i = 0; i < elevatorCount; i++) {
        if (elevators[i].lowest <= startFloor && startFloor <= elevators[i].highest &&
            elevators[i].lowest <= endFloor   && endFloor   <= elevators[i].highest) {

            int curFloor  = elevators[i].current;
            int remaining = 1; /* assume space available if query fails */

            if (api_get_elevator_status(elevators[i].id, status, sizeof(status)) == 0) {
                char bayID[MAX_NAME], dir[4];
                int pCount, remCap;
                /* format: "bayID|curFloor|dir|passengerCount|remainingCapacity" */
                if (sscanf(status, "%[^|]|%d|%[^|]|%d|%d",
                           bayID, &curFloor, dir, &pCount, &remCap) == 5) {
                    remaining = remCap;
                }
            }

            if (remaining <= 0) continue;

            int dist = abs(curFloor - startFloor);
            if (!foundCapacity || dist < bestDist) {
                bestDist      = dist;
                bestIdx       = i;
                foundCapacity = 1;
            }
        }
    }

    /* fallback: all elevators full — pick nearest by static floor */
    if (!foundCapacity) {
        bestDist = INT_MAX;
        for (int i = 0; i < elevatorCount; i++) {
            if (elevators[i].lowest <= startFloor && startFloor <= elevators[i].highest &&
                elevators[i].lowest <= endFloor   && endFloor   <= elevators[i].highest) {
                int dist = abs(elevators[i].current - startFloor);
                if (dist < bestDist) { bestDist = dist; bestIdx = i; }
            }
        }
    }

    return elevators[bestIdx].id;
}

/* polls /NextInput and pushes results to the input queue */
static void *input_thread(void *arg) {
    (void)arg;
    char response[MAX_RESPONSE];

    while (1) {
        api_get_next_input(response, sizeof(response));

        if (strcmp(response, "NONE") == 0) {
            if (simDone) break; /* sim over and queue empty — we're done */
            usleep(100000);
            continue;
        }

        PersonRequest req;
        sscanf(response, " %127[^ |] | %d | %d", req.personID, &req.startFloor, &req.endFloor);

        pthread_mutex_lock(&inputMutex);
        while (inputCount == QUEUE_SIZE && !simDone)
            pthread_cond_wait(&inputNotFull, &inputMutex);
        inputQueue[inputTail] = req;
        inputTail = (inputTail + 1) % QUEUE_SIZE;
        inputCount++;
        pthread_cond_signal(&inputNotEmpty);
        pthread_mutex_unlock(&inputMutex);
    }

    pthread_mutex_lock(&inputMutex);
    pthread_cond_broadcast(&inputNotEmpty);
    pthread_mutex_unlock(&inputMutex);
    return NULL;
}

/* picks an elevator for each person and pushes assignments to the output queue */
static void *scheduler_thread(void *arg) {
    (void)arg;

    while (1) {
        pthread_mutex_lock(&inputMutex);
        while (inputCount == 0 && !simDone)
            pthread_cond_wait(&inputNotEmpty, &inputMutex);

        if (inputCount == 0) {
            pthread_mutex_unlock(&inputMutex);
            break;
        }

        PersonRequest req = inputQueue[inputHead];
        inputHead = (inputHead + 1) % QUEUE_SIZE;
        inputCount--;
        pthread_cond_signal(&inputNotFull);
        pthread_mutex_unlock(&inputMutex);

        Assignment a;
        strncpy(a.personID,   req.personID,                            MAX_ID   - 1);
        strncpy(a.elevatorID, pick_elevator(req.startFloor, req.endFloor), MAX_NAME - 1);

        pthread_mutex_lock(&outputMutex);
        while (outputCount == QUEUE_SIZE && !simDone)
            pthread_cond_wait(&outputNotFull, &outputMutex);
        outputQueue[outputTail] = a;
        outputTail = (outputTail + 1) % QUEUE_SIZE;
        outputCount++;
        pthread_cond_signal(&outputNotEmpty);
        pthread_mutex_unlock(&outputMutex);
    }

    pthread_mutex_lock(&outputMutex);
    pthread_cond_broadcast(&outputNotEmpty);
    pthread_mutex_unlock(&outputMutex);
    return NULL;
}

/* calls /AddPersonToElevator for each assignment */
static void *output_thread(void *arg) {
    (void)arg;
    char response[MAX_RESPONSE];

    while (1) {
        pthread_mutex_lock(&outputMutex);
        while (outputCount == 0 && !simDone)
            pthread_cond_wait(&outputNotEmpty, &outputMutex);

        if (outputCount == 0) {
            pthread_mutex_unlock(&outputMutex);
            break;
        }

        Assignment a = outputQueue[outputHead];
        outputHead = (outputHead + 1) % QUEUE_SIZE;
        outputCount--;
        pthread_cond_signal(&outputNotFull);
        pthread_mutex_unlock(&outputMutex);

        api_add_person_to_elevator(a.personID, a.elevatorID, response, sizeof(response));
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <building_file> <port>\n", argv[0]);
        return 1;
    }

    FILE *buildingFile = fopen(argv[1], "r");
    if (buildingFile == NULL) {
        fprintf(stderr, "Error: could not open building file %s\n", argv[1]);
        return 1;
    }

    while (elevatorCount < MAX_ELEVATORS &&
           fscanf(buildingFile, "%s %d %d %d %d",
                  elevators[elevatorCount].id,
                  &elevators[elevatorCount].lowest,
                  &elevators[elevatorCount].highest,
                  &elevators[elevatorCount].current,
                  &elevators[elevatorCount].capacity) == 5) {
        elevatorCount++;
    }
    fclose(buildingFile);

    api_init();
    api_set_base_url(argv[2]);
    api_start_simulation();

    pthread_t tInput, tScheduler, tOutput;
    pthread_create(&tInput,     NULL, input_thread,     NULL);
    pthread_create(&tScheduler, NULL, scheduler_thread, NULL);
    pthread_create(&tOutput,    NULL, output_thread,    NULL);

    char status[MAX_RESPONSE];
    while (1) {
        usleep(500000);
        api_check_simulation(status, sizeof(status));
        if (strstr(status, "complete") || strstr(status, "stopped")) {
            simDone = 1;
            pthread_cond_broadcast(&inputNotEmpty);
            pthread_cond_broadcast(&inputNotFull);
            pthread_cond_broadcast(&outputNotEmpty);
            pthread_cond_broadcast(&outputNotFull);
            break;
        }
    }

    pthread_join(tInput,     NULL);
    pthread_join(tScheduler, NULL);
    pthread_join(tOutput,    NULL);

    api_cleanup();
    return 0;
}
