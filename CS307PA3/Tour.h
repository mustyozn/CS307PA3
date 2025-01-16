#include<stdio.h>
#include<pthread.h>
#include<semaphore.h>
#include<stdexcept>
/*
This is the implementation of Tour.h class
the aim of this class is to implement the concurrency logic to proivide touring 
that has no concurrency issues

*/

using namespace std;

class Tour {
public:
    // Constructor
    Tour(int visitorCount, int guide_required);

    // Destructor
    ~Tour();

    // Methods
    void arrive();  // Called when a visitor arrives
    void start();   // Placeholder, already defined in the test files
    void leave();   // Called when a visitor leaves

private:
    // Shared state variables
    //int total_num;         // Total number of threads
    int visitor_count;     // Number of visitors in a tour, excluding the guide
    int all_count;         // Number of visitors in a tour, including the guide
    //int num_tours;         // Number of tours completed
    int current_visitors;  // Current number of visitors at the site
    bool tour_in_progress; // True if a tour is ongoing
    bool has_guide;        // Indicates if a guide is needed for the tour


    // Synchronization primitives
    pthread_mutex_t visitor_mutex;       // Protect shared variables(used)
    //sem_t semaphore_arrive;              // Block new arrivals during a tour(no need)
    sem_t semaphore_tour;                // Allow visitors to start the tour(used)
    //pthread_barrier_t barrier_tour_start; // Synchronize tour start(no need)
    pthread_t guide_thread;              // Track the guide thread (if any)(used)
    pthread_cond_t cond_tour_end;//used

    
};



//constructor implementation
Tour::Tour(int visitorCount, int guide_required) {
    visitor_count = visitorCount; //number of visitors in the tour
    this->has_guide = (guide_required == 1); //check if guide is required
    current_visitors = 0; //since we have just created the tour there are no visitors yet
    tour_in_progress = false; //initially no tours have started hence it is false

    // 1. Input validation
    if (visitorCount <= 0) {
        throw std::invalid_argument("visitorCount must be greater than 0");
    }
    if (guide_required != 0 && guide_required != 1) {
        throw std::invalid_argument("Exception caught:  An error occurred.");
    }

    all_count = has_guide ? visitor_count + 1 : visitor_count; //if there needs to be a guide then the number of people in the guide need to be visitor_count + 1 else visitor_count
    //all_count = visitor_count;
    //initializing the visitor mutex
    if (pthread_mutex_init(&visitor_mutex, nullptr) != 0) {
        throw std::runtime_error("Failed to initialize visitor_mutex");
    }

    

    if (sem_init(&semaphore_tour, 0, 0) != 0) { // Blocked initially

        pthread_mutex_destroy(&visitor_mutex);
        throw std::runtime_error("Failed to initialize semaphore_tour");
    }

    

    if (pthread_cond_init(&cond_tour_end, nullptr) != 0) {
        sem_destroy(&semaphore_tour);

        pthread_mutex_destroy(&visitor_mutex);

        throw std::runtime_error("Failed to initialize cond_tour_end");
    }


    // Initialize guide_thread to a default value for safety
    guide_thread = pthread_t(); // This sets guide_thread to an uninitialized thread state
}




Tour::~Tour() {
    pthread_cond_destroy(&cond_tour_end);
    // Destroy the visitor mutex
    pthread_mutex_destroy(&visitor_mutex);

    // Destroy the semaphores
    sem_destroy(&semaphore_tour);

    
}
/*
Tour::arrive(){
    printf("Thread ID: %lu | Status: Arrived at the location.\n", pthread_self());

    pthread_mutex_lock(&visitor_mutex);//locking the visitor mutex
    //this uses semaphores to prevent busy-waiting, although there is a while loop, we immediately sleep if a tour starts and won't wake up until it ends.
    while(tour_in_progress){
        pthread_mutex_unlock(&visitor_mutex); // Release lock before blocking
        sem_wait(&semaphore_arrive);         // Block the thread so that it does busy wait
        pthread_mutex_lock(&visitor_mutex); // Reacquire lock after unblocking
    }
    //if we are here it means that the tour is not in progress, and we have gained back the access of the lock hence we are certain that only one thread can enter here
    current_visitors++;

    if(current_visitors == all_count){
        //if we are here it means that once we inserted the tour is now full
        //hence the tour must start
        if (has_guide) {
            guide_thread = pthread_self();
        }

        tour_in_progress = true;

        printf("Thread ID: %lu | Status: There are enough visitors, the tour is starting.\n", pthread_self());

        
    }else{
        
        printf("Thread ID: %lu | Status: Only %d visitors inside, starting solo shots.\n", pthread_self(), current_visitors);

    }
    pthread_mutex_unlock(&visitor_mutex);


}
*/


void Tour::arrive() {
    printf("Thread ID: %lu | Status: Arrived at the location.\n", pthread_self());

    pthread_mutex_lock(&visitor_mutex); // we need to use a mutex to prevent race conditions

    while(tour_in_progress) {
        //if we are here it means the there is tour in progress
        //pthread_mutex_unlock(&visitor_mutex); // Unlock before blocking
        //sem_wait(&semaphore_arrive);          // Block until the tour ends
        //pthread_mutex_lock(&visitor_mutex);   // Reacquire lock after unblocking
        pthread_cond_wait(&cond_tour_end, &visitor_mutex); // Wait until the tour ends
    }

    //if we are here it means that no tour is in progress
    current_visitors++; //increment the current number of visitors

    if (current_visitors == all_count) {
        //if we are here it means that the tour capcity is full
        if (has_guide) {
            //if we are here it means the tour contains a guide
            guide_thread = pthread_self(); //the last visitor is assigned as a guide
        }

        printf("Thread ID: %lu | Status: There are enough visitors, the tour is starting.\n", pthread_self());
        tour_in_progress = true;

        // Synchronize all visitors for the tour start
        //pthread_barrier_wait(&barrier_tour_start);
        //********************
        //pthread_mutex_unlock(&visitor_mutex);
        // Synchronize all visitors for the tour start (I am not sure about this one, I will check it later on)
        //pthread_barrier_wait(&barrier_tour_start);
    } else {
        //if we are here it means that there are not enough visitors in the tour
        printf("Thread ID: %lu | Status: Only %d visitors inside, starting solo shots.\n", pthread_self(), current_visitors);

        // Release the mutex so the visitor can decide to leave later
        //pthread_mutex_unlock(&visitor_mutex);
    }

    pthread_mutex_unlock(&visitor_mutex); // Unlock mutex
}




void Tour::leave() {
    pthread_mutex_lock(&visitor_mutex);//this mutex is only used for ensuring atomicity of decrement operations, hence there will be no busy waiting

    if (!tour_in_progress) {
        //if we are here it means that the tour is not in progress so the visitors can leave whenever they want
        printf("Thread ID: %lu | Status: My camera ran out of memory while waiting, I am leaving.\n", pthread_self());
        if (current_visitors > 0) {
            current_visitors--; // Decrement the visitor count
        }
        pthread_mutex_unlock(&visitor_mutex); // Unlock and exit
        return; //the visitor has left hence we return from the function
    }

    if (has_guide && pthread_equal(pthread_self(), guide_thread)) {
        //if we are here it means that this is the guide and the guide finishes the tour
        printf("Thread ID: %lu | Status: Tour guide speaking, the tour is over.\n", pthread_self());
        current_visitors--; // Decrement the visitors count
        sem_post(&semaphore_tour); // Unblocking the waiting visitors (that are in the tour and waiting for the guide to leave)
    } else {
        if (has_guide) {
            //if we are here it means the guide exists in the tour hence we need to wait until the guide leaves
            pthread_mutex_unlock(&visitor_mutex); //unlocking the mutex before blocking
            sem_wait(&semaphore_tour);           // Wait for the guide to leave
            pthread_mutex_lock(&visitor_mutex);  // Reacquiring the lock once the guide leaves the tour
        } 
        //if we are here, it means that the tour had no guide or the visitor is leaving after the guide
        printf("Thread ID: %lu | Status: I am a visitor and I am leaving.\n", pthread_self());
        if (current_visitors > 0) {
            current_visitors--; // Decrement the visitors count
        }
        if (current_visitors > 0) {
            sem_post(&semaphore_tour); // Unblock the next visitor in line
        }
    }

    if (current_visitors == 0) {
        //if we are here it means the last visitor is leaving hence we announce that new visitors can come
        printf("Thread ID: %lu | Status: All visitors have left, the new visitors can come.\n", pthread_self());
        tour_in_progress = false; // Reset the tour status
        pthread_cond_broadcast(&cond_tour_end);
       
        //sem_post(&semaphore_arrive); // Unblock any visitors waiting to enter
       // pthread_cond_broadcast(&cond_tour_end);
    }

    pthread_mutex_unlock(&visitor_mutex); // Unlock the mutex
}





/*
Tour::leave(){
    pthread_mutex_lock(&visitor_mutex);

    if(!tour_in_progress){
        //if the tour has not started and the visitor wants to leave 
        printf("Thread ID: %lu | Status: My camera ran out of memory while waiting, I am leaving.\n", pthread_self());
        current_visitors--; // Decrement the visitor count
        pthread_mutex_unlock(&visitor_mutex); // Release the mutex
        return; // Exit the function
    }


    //if tour is in progress 
    //we need to check if there is a guide, because the guide leaves first
    if (has_guide && pthread_equal(guide_thread, pthread_self())) {
        printf("Thread ID: %lu | Status: Tour guide speaking, the tour is over.\n", pthread_self());
    } else {
        // The current thread is a regular visitor
        printf("Thread ID: %lu | Status: I am a visitor and I am leaving.\n", pthread_self());
    }

    // Decrement the visitor count
    current_visitors--;


    // Check if the current thread is the last visitor
    if (current_visitors == 0) {
        printf("Thread ID: %lu | Status: All visitors have left, the new visitors can come.\n", pthread_self());
        tour_in_progress = false; // Reset the tour status
        sem_post(&semaphore_arrive); // Signal waiting threads to enter
    }

    pthread_mutex_unlock(&visitor_mutex); // Release the mutex



}
*/
