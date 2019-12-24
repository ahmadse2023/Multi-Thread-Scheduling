#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>

extern int errno;

typedef struct {
    int id;
    char priority;
    int loading_time;
    int crossing_time;
} train;
int east_rank;
int west_rank;
int num_sent = 0;
pthread_mutex_t lock_track_mux, lock_station_mux;
int broadcast = 0;
int next_index;
train station[1000];
train initial_trains[1000];
pthread_t threads[1000];
int station_index = 0;

struct timespec start, finish;
long nseconds;
long seconds;

// if last = 0 = e || last = 1 = w

int dispatcher(int last) {
    int west_found = 0;
    int east_found = 0;
    pthread_mutex_lock(&lock_station_mux);
    train next = station[0];
    int next_index = 0;
    if (east_rank >= 3) {

        for (int i = 1; i < station_index; i++) {
            train this = station[i];
            if (tolower(next.priority) == 'e' && tolower(this.priority) == 'w') {
                next = this;
                next_index = i;
                west_found = 1;
                continue;
            }
            else if (next.priority == 'w' && this.priority == 'W') {
                next = this;
                next_index = i;
                west_found = 1;
                continue;
            }
        }
        if (west_found) {
            pthread_mutex_unlock(&lock_station_mux);
            return next_index;
        }
    }
    else if (west_rank >= 3) {

        for (int i = 1; i < station_index; i++) {
            train this = station[i];
            if (tolower(next.priority) == 'w' && tolower(this.priority) == 'e') {
                this = next;
                next_index = i;
                east_found = 1;
                continue;
            }
            else if (next.priority == 'e' && this.priority == 'E') {
                next = this;
                next_index = i;
                east_found = 1;
                continue;
            }
        }
        if (east_found) {
            pthread_mutex_unlock(&lock_station_mux);
            return next_index;
        }
    }

    else {
        next = station[0];
        next_index = 0;
        for (int i = 1; i < station_index; i++) {
            train this = station[i];
            if (isupper(this.priority) && islower(next.priority)) {
                next = this;
                next_index = i;
            } else if (islower(this.priority) && isupper(next.priority)) {
                continue;
            } else if ((isupper(this.priority) && isupper(next.priority)) ||
                       (islower(this.priority) && islower(next.priority))) {
                if (this.priority == next.priority) {
                    int id = this.id;
                    if (id < next.id && this.loading_time == next.loading_time) {
                        next = this;
                        next_index = i;
                    }
                    continue;
                } else {
                    if (num_sent == 0) {
                        if (next.priority == 'E') {
                            continue;
                        }
                        else {
                            next = this;
                            next_index = i;
                            continue;
                        }
                    }
                    else if (last == 0) {
                        if (toupper(this.priority) == 'W') {
                            next = this;
                            next_index = i;
                            continue;
                        } else {
                            continue;
                        }
                    } else if (last == 1) {
                        if (toupper(this.priority) == 'E') {
                            next = this;
                            next_index = i;
                            continue;
                        } else {
                            continue;
                        }
                    }
                }
            }
        }
    }
    pthread_mutex_unlock(&lock_station_mux);
    return next_index;
}


int count_lines(FILE * fp, int * low_e, int * low_w, int * high_e, int * high_w) {
    while (!feof(fp)) {
        int ch = tolower(fgetc(fp));
        if (ch == 'e') {
            *low_e = *low_e + 1;
        }
        else if (ch == 'E') {
            *high_e = * high_e + 1;
        }
        else if (ch == 'w') {
            *low_w = *low_w + 1;
        }
        else if (ch == 'W') {
            *high_w = *high_w + 1;
        }
    }
    return *low_e + *high_e + *low_w + *high_w;
}

void display_time() {
    clock_gettime( CLOCK_REALTIME, &finish);
    long hours;
    long minutes;
    seconds = finish.tv_sec - start.tv_sec;
    nseconds = finish.tv_nsec - start.tv_nsec;
    // adopted from http://www.cs.tufts.edu/comp/111/examples/Time/clock_gettime.c
    if (start.tv_nsec > finish.tv_nsec) { // clock underflow
        --seconds;
        nseconds += 1000000000;
    }
    long deciseconds = nseconds / 100000000; // convert nanoseconds to deciseconds
    if (seconds > 60) {
        hours = seconds / 3600;
        minutes = (seconds / 60) % 60;
        seconds = seconds % 60;
    }
    else {
        hours = 0;
        minutes = 0;
    }
    printf("%02ld:%02ld:%02ld.%ld ", hours, minutes, seconds, deciseconds);
}

FILE * open_file(char * filename) {
    FILE * fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("Error opening file: %s", strerror(errno));
        exit(0);
    }
    return fp;
}

void initialize_trains(FILE * fp, train * trains, int num_trains) {
    int trains_index = 0;
    while(trains_index < num_trains) {
        train * ptr = trains + trains_index;
        (*ptr).id = trains_index;
        fscanf(fp, "%c %d %d\n", &(*ptr).priority, &(*ptr).loading_time, &(*ptr).crossing_time);
        trains_index++;
    }
}

void * loading_thread(void * arg) {
    while (!broadcast) {
        // wait until broadcasting begins
    }
    train * this_train = (train *)arg;
    usleep(this_train->loading_time * 100000);
    pthread_mutex_lock(&lock_station_mux);
    display_time();
    char ch = this_train->priority;
    if (ch == 'w' || ch == 'W') {
        printf("Train %d is ready to go West\n", this_train->id);
    }
    else {
        printf("Train %d is ready to go East\n", this_train->id);
    }

    station[station_index] = *this_train;
    station_index++;
    pthread_mutex_unlock(&lock_station_mux);
    pthread_exit(0);
}
void remove_train(int index) {

    pthread_mutex_lock(&lock_station_mux);

    if (index == station_index-1) {
        // skip
    } //
    else if (station_index != 1) {
        int i = 0;
        for (; i < index; i++) {}
        for (; i < station_index - 1; i++) {
            station[i] = station[i + 1];
        }
    }
    station_index--;
    pthread_mutex_unlock(&lock_station_mux);
}
void * crossing_thread(void * arg) {
    pthread_mutex_lock(&lock_track_mux);
    train * this_train = (train *)arg;
    char ch = this_train->priority;
    display_time();
    if (ch == 'w' || ch == 'W') {
        printf("Train %d is ON the main track going West\n", this_train->id);
    }
    else {
        printf("Train %d is ON the main track going East\n", this_train->id);
    }
    usleep(this_train->crossing_time * 100000); // convert deciseconds to microseconds and sleep (cross) for that time
    display_time();
    if (ch == 'w' || ch == 'W') {
        printf("Train %d is OFF the main track after going West\n", this_train->id);
    }
    else {
        printf("Train %d is OFF the main track after going East\n", this_train->id);
    }
    num_sent++;
    pthread_mutex_unlock(&lock_track_mux);
    remove_train(next_index);
    pthread_exit(0);
}

void initialize_threads(pthread_t * trains_threads, train * trains, int num_trains) {
    for (int i = 0; i < num_trains; i++) {
        pthread_create(trains_threads + i, NULL, loading_thread, (void *)(trains + i));
    }
}

int main(int argc, char* args[]) {
    east_rank = 0;
    west_rank = 0;
    int last = 2; // assigned randomly; last == 1 means 'w' || 'W', last == 0 means 'e' || 'E'
    pthread_mutex_init(&lock_track_mux, NULL);
    pthread_mutex_init(&lock_station_mux, NULL);

    FILE * fp = open_file(args[1]);
    int low_w = 0;
    int high_w = 0;
    int low_e = 0;
    int high_e = 0;
    int num_lines = count_lines(fp, &low_e, &low_w, &high_e, &high_w);
    rewind(fp);

    initialize_trains(fp, initial_trains, num_lines);
    clock_gettime( CLOCK_REALTIME, &start);
    initialize_threads(threads, initial_trains, num_lines);

    broadcast = 1; // begin broadcasting

    while (num_sent < num_lines) {
        while (station_index == 0) {
            // do nothing
        }
        next_index = dispatcher(last);

        char ch = station[next_index].priority;
        if (toupper(ch) == 'E') {
            last = 0;
            east_rank++;
            west_rank = 0;
        } else {
            last = 1;
            east_rank = 0;
            west_rank++;
        }


        pthread_t a;
        pthread_create(&a, NULL, crossing_thread, (void *)(station + next_index));
        pthread_join(a, NULL);
    }
}
