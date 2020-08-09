#include <bits/stdc++.h>
#include <pthread.h>
#include <signal.h>
#include <ctime>
#include <unistd.h> 

#define MAX_QUEUE_SIZE 3000
#define RAND_NUMS 1000
#define RUNNING 0
#define WAITING 1
#define TERMINATED 2
#define STEP 2
#define MAX_THREAD_COUNT 20

using namespace std;

queue<int> BUFFER;
vector<int> STATUS(MAX_THREAD_COUNT, 0);
vector<int> w_type;
pthread_t WORKERS[MAX_THREAD_COUNT], SCHEDULER, REPORTER;
pthread_mutex_t mtx, stat_lock;

const int range_from  = 0;
const int range_to    = 1;
random_device                  rand_dev;
mt19937                        generator(rand_dev());
uniform_int_distribution<int>  distr(range_from, range_to);

int n_prod = 0, n_cons = 0, job_comp = 0;

void WAIT(int id)
{
    pause();
    return;
}

void RESUME(int id)
{
    return;
}

void *produce(void *param)
{
    int ID = *(int *)param;
    for(int i = 0; i < RAND_NUMS; i++)
    {
        while(BUFFER.size() >= MAX_QUEUE_SIZE && n_cons);

        pthread_mutex_lock(&mtx);
        BUFFER.push(rand()%MAX_QUEUE_SIZE);

        pthread_mutex_unlock(&mtx);
        usleep(1000);
    }

    // update status
    STATUS[ID] = TERMINATED;

    pthread_exit(EXIT_SUCCESS);
}

void *consume(void *param)
{
    int ID = *(int *)param;
    while(1)
    {
        while(BUFFER.empty())
        {
            if(job_comp >= RAND_NUMS*n_prod) break;
        }

        if(BUFFER.empty()) break;
        pthread_mutex_lock(&mtx);
        BUFFER.pop();
        job_comp++;
        pthread_mutex_unlock(&mtx);
        usleep(1000);
    }

    // update status
    STATUS[ID] = TERMINATED;

    pthread_exit(EXIT_SUCCESS);
}

bool assign_work(int ID)
{
    // coin toss
    bool heads = (distr(generator) > 0.5);
    
    return heads;
}

void *status_reporter(void *param)
{
    usleep(100000);
    int N = *(int *)param;
    vector<int> prev_status(N, WAITING);
    
    vector<string> name = {"RUNNING", "WAITING", "TERMINATED"};
    int w_comp = 0;
    while(w_comp < N)
    {
        pthread_mutex_lock(&stat_lock);
        bool completed = true;
        for(int i = 0; i < N; i++)
        {
            vector<string> w_tp = {"Consumer", "Producer"};
            if(prev_status[i] != STATUS[i])
            {
                if(prev_status[i] == WAITING && STATUS[i] == TERMINATED)
                {
                    cout << "[" << w_tp[w_type[i]] << " ID: " << i + 1 << "]: [WAITING] -> [RUNNING]" << endl;
                    cout << "[" << w_tp[w_type[i]] << " ID: " << i + 1 << "]: [RUNNING] -> [TERMINATED]" << endl;
                }
                else cout << "[" << w_tp[w_type[i]] << " ID: " << i + 1 << "]: [" << name[prev_status[i]] 
                          << "] -> [" <<  name[STATUS[i]] << "]" << endl;
                cout << "BUFFER Size: " << BUFFER.size() << endl;
                completed = false;
                prev_status[i] = STATUS[i];

                if(STATUS[i] == TERMINATED) w_comp++;
            }
        }
        pthread_mutex_unlock(&stat_lock);    
    }
    pthread_exit(EXIT_SUCCESS);
}

void *rr_scheduler(void *param)
{
    usleep(100000);
    int N = *(int *)param;
    queue<int> ready_q;
    for(int i = 0; i < N; i++) ready_q.push(i);

    while(!ready_q.empty())
    {
        int curr_w = ready_q.front();
        ready_q.pop();
        // start the thread
        pthread_mutex_lock(&stat_lock);
        STATUS[curr_w] = RUNNING;
        pthread_kill(WORKERS[curr_w], SIGUSR2);
        pthread_mutex_unlock(&stat_lock);

        sleep(STEP);
        
        pthread_mutex_lock(&stat_lock);
        if(ready_q.empty()) while(STATUS[curr_w] != TERMINATED);

        if(STATUS[curr_w] != TERMINATED)
        {
            pthread_kill(WORKERS[curr_w], SIGUSR1);
            ready_q.push(curr_w);
            STATUS[curr_w] = WAITING;
        }
        pthread_mutex_unlock(&stat_lock);
        usleep(1000);
    }
    pthread_exit(EXIT_SUCCESS);
}   

int main()
{
    srand(time(NULL));

    int N;
    cout << "Enter no. of worker threads: ";
    cin >> N;

    if(N > MAX_THREAD_COUNT)
    {
        cout << "Please enter <= " << MAX_THREAD_COUNT << " threads" << endl;
        return EXIT_FAILURE;
    }
    if(pthread_mutex_init(&mtx, NULL) || pthread_mutex_init(&stat_lock, NULL))
    {
        perror("Mutex init failed");
        exit(EXIT_FAILURE);
    }

    int p_tstat;
    int args[N];
    w_type.resize(N);
    for(int i = 0; i < N; i++)
    {
        w_type[i] = assign_work(i) == true;
        if(w_type[i]) n_prod++;
    }
    n_cons = N - n_prod;

    signal(SIGUSR1, WAIT);
    signal(SIGUSR2, RESUME);
    for(int i = 0; i < N; i++)
    {
        args[i] = i;
        
        if(w_type[i])
        {
            if((p_tstat = pthread_create(&WORKERS[i], NULL, &produce, (void *)(args + i))))
            {
                perror("Worker thread: creation failed");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            if((p_tstat = pthread_create(&WORKERS[i], NULL, &consume, (void *)(args + i))))
            {
                perror("Worker thread: creation failed");
                exit(EXIT_FAILURE);
            }
        }

        // update status
        pthread_mutex_lock(&stat_lock);
        STATUS[i] = WAITING;
        // signal the thread to wait 
        pthread_kill(WORKERS[i], SIGUSR1);
        pthread_mutex_unlock(&stat_lock);
        
    }

    cout << "No. of Producers: " << n_prod << endl;
    if((p_tstat = pthread_create(&SCHEDULER, NULL, &rr_scheduler, (void *)&N)))
    {
        perror("Scheduler thread: creation failed");
        exit(EXIT_FAILURE);
    }
    
    if((p_tstat = pthread_create(&REPORTER, NULL, &status_reporter, (void *)&N)))
    {
        perror("Reporter thread: creation failed");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < N; i++) pthread_join(WORKERS[i], NULL);
    pthread_join(SCHEDULER, NULL);
    pthread_join(REPORTER, NULL);

    cout << "Completed... Exiting" << endl;
    return EXIT_SUCCESS;
}