#include <iostream>
#include <thread>
#include <chrono>
#include <functional>
#include "config.h"

using namespace std;
vector<thread> threads; //global thread container

void spawnThreads(int threadCount, function<void(int)> task){ //NOTE: update function variable when adding params/changing return of the task division scheme
    for(int i=0; i<threadCount; i++){
        threads.emplace_back(task, numY); //add to this if the task division functions need more variables (figure this out)
    }
}

void joinThreads(){
    for(thread &thread : threads){
        thread.join();
    }
}

//Task division scheme: straight division of search range
void threadByRange(int maxNum){} //TODO!!!

//Task division scheme: threads are for divisibility testing
void threadByDiv(int maxNum){}


int main(){

spawnThreads(threadX, threadByRange);
joinThreads();


}