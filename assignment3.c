#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <sys/types.h>
#include <string.h>

#define NUM_CONSUMER_THREADS 4
#define NUM_CORES 4
#define MAX_PROCESS 50
#define MAX(a, b) ((a) > (b) ? (a) : (b)) 
#define MIN(a, b) ((a) < (b) ? (a) : (b))


// structures and enums
enum SchedulingType {
    NORMAL,
    RR,
    FIFO
};

struct data {
	int pid;
    int sp;
    int dp;
    int remain_time;
    int time_slice;
    int accu_time_slice;
    int last_cpu;
    int execution_time; 
    int cpu_affinity;
    enum SchedulingType policy;
    int avg_sleep;
    int process_blocked;
    struct timeval time_blocked;
};

struct core {
    struct data RQ_0[MAX_PROCESS];
    struct data RQ_1[MAX_PROCESS];
    struct data RQ_2[MAX_PROCESS];
};




// Global variables
pthread_mutex_t mutex_lock;
struct core cores[NUM_CORES];
void *thread_function_p(void *arg);
void *thread_function_c(void *arg);
int global = 0;
struct data buffer[NUM_CONSUMER_THREADS];
int next_random_CPU = 0;
int running = 1;
int max_sleep = 10;




// deciphering enum
enum SchedulingType get_scheduling_type(const char *policy_str) {
    if (strcmp(policy_str, "NORMAL") == 0) {
        return NORMAL;
    } else if (strcmp(policy_str, "RR") == 0) {
        return RR;
    } else if (strcmp(policy_str, "FIFO") == 0) {
        return FIFO;
    } else {
        // Default to NORMAL if the string doesn't match any known type
        return NORMAL;
    }
}


// Function to parse a CSV line and store the data in the struct
int parse_csv_line(FILE *file, struct data *process_data) {
    char policy_str[7]; // Temporary buffer for the policy string

    int result = fscanf(file, "%d,%6[^,],%d,%d,%d",
                        &process_data->pid,
                        policy_str,
                        &process_data->sp,
                        &process_data->execution_time,
                        &process_data->cpu_affinity);

    if (result == 0) {
        // fscanf returns 0 when it fails to match any items
        return 0;
    } else if (result != 5) {
        // Parsing error
        return -1;
    }

    // Convert the policy string to the corresponding enum value
    process_data->policy = get_scheduling_type(policy_str);

    return 1; // Success
}


int calculate_time_slice(int time_slice) {
    if(time_slice<120){
           return (140-time_slice) * 20;
        }
        else{
            return  (140-time_slice) * 5;
        }

}

// prints the data/pcb of the process
void print_data(struct data datap){ 
    printf("\t\tpid: %d\n",datap.pid);
    printf("\t\tstatic priority: %ld\n",datap.sp);
    printf("\t\tdynamic priority: %d\n",datap.dp);
    printf("\t\taccu time slice %d\n",datap.accu_time_slice);
    printf("\t\tlast CPU %d\n",datap.last_cpu);
    printf("\t\tExecution time %d\n",datap.execution_time);
    printf("\t\tRemain time %d\n", datap.remain_time);
    printf("\t\tcpu affinity %d\n\n",datap.cpu_affinity);
}

int calculate_dyanmic_priority(int sp, int bonus){
    return MAX(100, MIN(sp - bonus + 5, 139));
}

// generates a random integer
int generate_int(int min, int max) {
    if (min >= max) {min = max;}
    return (rand() % (max - min + 1)) + min;
}

// gives you the next available produce index(empty space) if producing and next available consume index if consuming
int next_queue_index(struct data *queue, int consuming) {
    for (int i = 0; i<MAX_PROCESS; i++) {
        if (queue[i].pid > 0) {
            if (consuming > 0) {
                
                return i;
            }
            
        } else {
            
            if (consuming <= 0) {
                
                return i;
            }
        } 
        
    }
    return -1;
}

long double calculate_delta(struct timeval start, struct timeval end){
    return ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
}


int pick_core_index(int affinity) {
    if (affinity == -1){
        //pick any CPU
        next_random_CPU = (next_random_CPU + 1) % 4;
        return next_random_CPU;
    } else if (affinity == 0) {return 0;}
    else if (affinity == 1) {return 1;}
    else if (affinity == 2) {return 2;}
    else {return 3;}

}

int pick_ready_queue(int priority)
{
    if (priority >= 0 && priority < 100){return 0;}
    else if (priority >= 100 && priority < 130){return 1;}
    else if (priority >= 130 && priority < 140){return 2;}
    else {
        perror("Invalid Priority");
        exit(EXIT_FAILURE);
    }
}

int calculate_average_sleep(int sleep_avg, long double delta_time, int time_quantum){
    sleep_avg += delta_time / time_quantum - 1; 
    if(sleep_avg < 0) {
        sleep_avg = 0;
    } else if( sleep_avg > max_sleep ){
        sleep_avg = max_sleep;
    }
    return (int) sleep_avg;
}

// main function
int main() {
    int res;
    int thread_num;
    pthread_t parent;
    pthread_t a_thread[thread_num];
    void *thread_return;   
    
    
    pthread_attr_t thread_attr;

    

   
    if(pthread_mutex_init(&mutex_lock, NULL) != 0) {perror("Mutex initialization failed"); exit(EXIT_FAILURE);} // change
   // create producer thread
    res = pthread_create(&parent,NULL,thread_function_p,NULL);
    if (res != 0) {
        perror("Thread creation failed");
        exit(EXIT_FAILURE);
    }

    usleep(100); // allow producuer to produce a bit
	

    // create CPU threads
    for(int CPU_num = 0; CPU_num < NUM_CONSUMER_THREADS; CPU_num++) {
        res = pthread_create(&(a_thread[CPU_num]), NULL, thread_function_c, (void *)&CPU_num);
        if (res != 0) {
            perror("Thread creation failed");
            exit(EXIT_FAILURE);
        }
        usleep(600);
        
    }

    // join CPU threads
    for(int CPU_num = 0; CPU_num < NUM_CONSUMER_THREADS; CPU_num++){
        
        pthread_t *thread_c = &a_thread[CPU_num];
        pthread_join(*thread_c, NULL); 
    }
    
    // join parent thread
    if (pthread_join(parent, &thread_return) != 0)
    {
        perror("Parent Thread join failed");
        exit(EXIT_FAILURE);
    }

    sleep(2);
    
    printf("\n\n\nAll done\n");

    pthread_mutex_destroy(&mutex_lock);



    exit(EXIT_SUCCESS);
}

//  CPU
void *thread_function_c(void *arg) {  
    int my_number = *(int *)arg;
    struct timeval UNBLOCKED_TIME;
    long double delta;
    printf("\n\nmade it to CPU %d", my_number);

    // struct data *data= (struct data *)arg;
    while (running) {
    
        struct data *data1;
        
        int process_index;

        while (global<7) {} // wait for the producer to produce 7 times

        int working_queue = -1;

        // checks the queues for the next process
        while(working_queue==-1) {
            
            if ( next_queue_index(cores[my_number].RQ_0, 1) >= 0) {
                working_queue = 0;
                process_index = next_queue_index(cores[my_number].RQ_0, 1);
                break;
                
            } else if (next_queue_index(cores[my_number].RQ_1, 1) >= 0) {
                working_queue = 1;
                process_index = next_queue_index(cores[my_number].RQ_1, 1);
                break;
            } else if (next_queue_index(cores[my_number].RQ_2, 1) >= 0) {
                working_queue = 2;
                process_index = next_queue_index(cores[my_number].RQ_2, 1);
                break;
            } else {
                continue;
                
            }

        }
        

        pthread_mutex_lock(&mutex_lock);
        printf("\t\tCPU %d has locked the mutex\n", my_number);
        // aquire a process from the queues
        if (working_queue==0) {data1 = &cores[my_number].RQ_0[process_index];}
        if (working_queue==1) {data1 = &cores[my_number].RQ_1[process_index];}
        if (working_queue==2) {data1 = &cores[my_number].RQ_2[process_index];}

        // if it's blocked get the time it slept
        if (data1->process_blocked && data1->policy == NORMAL) {
            gettimeofday(&UNBLOCKED_TIME, NULL);
            delta = calculate_delta(data1->time_blocked, UNBLOCKED_TIME);
        }
        
        data1->time_slice = calculate_time_slice(data1->sp);
        int bonus = 10;
        
        data1->last_cpu = my_number; 

        if (data1->policy == NORMAL) { // Normal queue process
            printf("\t\tNormal process found\n");
            data1->execution_time = generate_int(1,10) * 10; 
            if(data1->execution_time>data1->time_slice){
                data1->execution_time = data1->time_slice;
            }
            data1->avg_sleep =  calculate_average_sleep(data1->avg_sleep, delta, data1->time_slice);
            
            // if process stops before allocated time slice
            if(data1->execution_time < data1->time_slice){
                data1->process_blocked = 1;
                printf("\t\tprocess stopped before Time Slice, blocking...\n");
                gettimeofday(&data1->time_blocked, NULL); 
                usleep(generate_int(100,600)); 
            }

            // if process is done
            if(data1->remain_time <= 0) {
                data1->process_blocked = 0;
                printf("\t\tprocess completed\n");
                printf("\nCPU %d returned: \n", my_number + 1);
                print_data(*data1);
                data1->pid = -1;

            } else { // process isn't done, requeue it 
                data1->dp = calculate_dyanmic_priority(data1->dp, data1->avg_sleep);
                struct data data2 = *data1;
                printf("\nCPU %d returned: \n", my_number + 1);
                print_data(*data1);
                printf("\t\tProcess is Normal, Process time slice exceeded, requeueing...\n");
                data1->pid = -1;

                if (data1->dp >= 130) {

                    
                    cores[my_number].RQ_2[next_queue_index(cores[my_number].RQ_2, 0)] = data2;

                } else {
                    
                    if (working_queue==0) {cores[my_number].RQ_0[next_queue_index(cores[my_number].RQ_0, 0)] = data2;}
                    if (working_queue==1) {cores[my_number].RQ_1[next_queue_index(cores[my_number].RQ_1, 0)] = data2;}
                    if (working_queue==2) {cores[my_number].RQ_2[next_queue_index(cores[my_number].RQ_2, 0)] = data2;}


                }


                
            }


        } else if(data1->policy == RR) {// round robin queue process
            printf("\t\tRound robin process found\n");
            struct data data2 = *data1;
            
            usleep(data1->time_slice);
            data1->accu_time_slice += data1->time_slice;
            
            // process won't complete in time slice
            if(data1->remain_time - data1->time_slice>=0){
                data1->remain_time -= data1->time_slice;
            }
            // process is done
            if(data1->remain_time <= 0){
                printf("\t\tprocess completed\n");
                printf("\nCPU %d returned: \n", my_number + 1);
                print_data(*data1);
                
                data1->pid = -1;
            } else {// requeue process
                printf("\t\tProcess is RR, Process time slice exceeded, requeueing...\n");
                print_data(*data1);
                data1->pid = -1;
                if (working_queue==0) {cores[my_number].RQ_0[next_queue_index(cores[my_number].RQ_0, 0)] = data2;}
                if (working_queue==1) {cores[my_number].RQ_1[next_queue_index(cores[my_number].RQ_1, 0)] = data2;}
                if (working_queue==2) {cores[my_number].RQ_2[next_queue_index(cores[my_number].RQ_2, 0)] = data2;}


            }

        } else { // FIFO queue process
            printf("\t\tFIFO process found\n");
            printf("Is remaining time causing the issue? here is remaining time %d \n", data1->remain_time);
            usleep(data1->execution_time );
            data1->accu_time_slice += data1->time_slice;

            
            printf("\t\tprocess completed\n");
            printf("\nCPU %d returned: \n", my_number + 1);
            print_data(*data1);

            // "consume" process
            data1->pid = -1;


        }
        
        pthread_mutex_unlock(&mutex_lock);
        printf("CPU %d has unlocked the mutex\n", my_number);
    }
 
    pthread_exit(NULL);
}


void *thread_function_p(void *arg) {

    
    FILE *file;

    struct data store_data;

    file = fopen("pcb_data.csv", "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    int i = 0;

    
    while (running) {
        int check = parse_csv_line(file, &store_data);
        if(check == 1) {
            
            // what the producer read from the CSV
            printf("\nOS Has recieved process:\n\tPID: %d\n\tPolicy: %d\n\tPriority: %d\n\tExecution Time: %d\n\tCPU Affinity: %d\n", store_data.pid, store_data.policy, store_data.sp, store_data.execution_time, store_data.cpu_affinity);
            store_data.time_slice = 0;
            store_data.accu_time_slice = 0;
            store_data.last_cpu = -1;
            store_data.avg_sleep = 0;

            int picked_core = pick_core_index(store_data.cpu_affinity);
            int picked_queue = pick_ready_queue(store_data.sp);
            printf("OS has picked core: %d, and picked queue: %d\n", picked_core + 1, picked_queue); // which core/queue was picked by the producer

            pthread_mutex_lock(&mutex_lock);

            // add the process to the appropriate queue and core
            if (picked_queue==0){
                cores[picked_core].RQ_0[next_queue_index(cores[picked_core].RQ_0, 0)] = store_data;
            } else if (picked_queue==1) {
                cores[picked_core].RQ_1[next_queue_index(cores[picked_core].RQ_1, 0)] = store_data;
            } else if (picked_queue == 2) {
                cores[picked_core].RQ_2[next_queue_index(cores[picked_core].RQ_2, 0)] = store_data;
            }
            
            pthread_mutex_unlock(&mutex_lock);

        }
        else if (check == 0)
        {
            // End of file or invalid input line
            running = 0;
            break;
        }
        else
        {
            // Parsing error
            fprintf(stderr, "Error parsing CSV line\n");
            running = 0;
            break;
        }

        global ++;

        usleep((100));

    }
    
    printf("producer thread is done producing");

    fclose(file);

    pthread_exit(NULL);
}
