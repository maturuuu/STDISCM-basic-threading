#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <math.h>
#include <mutex>
#include "config.h"

using namespace std;
mutex printMutex;

vector<thread> threads; //global thread container
chrono::steady_clock::time_point startTime; //holds the start time, updated per run

string getTimestamp(){
    auto now = chrono::steady_clock::now();
    auto diff = now - startTime;
    ostringstream timeString;

    auto hours   = duration_cast<chrono::hours>(diff).count();
    auto minutes = duration_cast<chrono::minutes>(diff).count() % 60;
    auto seconds = duration_cast<chrono::seconds>(diff).count() % 60;
    auto millis  = duration_cast<chrono::milliseconds>(diff).count() % 1000;

    timeString << setw(2) << setfill('0') << hours << ":"
                << setw(2) << setfill('0') << minutes << ":"
                << setw(2) << setfill('0') << seconds << ":"
                << setw(3) << setfill('0') << millis;

    return timeString.str();
}

string getCurrentTime(){
    auto now = chrono::system_clock::now();
    auto millis = duration_cast<chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    ostringstream timeNow;

    time_t rawTime = chrono::system_clock::to_time_t(now);
    tm localTime{};
    localtime_s(&localTime, &rawTime); //this will only work on windows

    timeNow << put_time(&localTime, "%H:%M:%S") << ":"
            << setw(3) << setfill('0') << millis;

    return timeNow.str();
}

void joinThreads(){
    for(thread &thread : threads){
        thread.join();
    }
}

bool checkIfPrime(int num){
    if(num == 0 || num == 1) return false;
    if(num == 2) return true;

    for(int i = 2; pow(i, 2) <= num; i++){
        if(num % i == 0) return false;
    }

    return true;
}

void checkIfPrimeRange(int start, int end, int id){
    for(int num=start; num<end; num++){
        if(checkIfPrime(num) == true){
            string timestamp = getTimestamp();
            lock_guard<mutex> lock(printMutex);
            cout << timestamp << " | " << "Thread " << id << " checking number " << num << " -> PRIME" << "\n";
        }
        else {
            string timestamp = getTimestamp();
            lock_guard<mutex> lock(printMutex);
            cout << timestamp << " | " << "Thread " << id << " checking number " << num << " -> NOT PRIME" << "\n";
        }
    }
}

//Task division scheme: straight division of search range (print immediately) DONE!!
void threadByRange1(int threadCount, int maxNum){
    int adjustedMaxNum = maxNum + 1;

    int rangeSize = (adjustedMaxNum + threadCount - 1) / threadCount;

    for(int i=0; i<threadCount; i++){
        int start = i * rangeSize;
        int end = min(start + rangeSize, adjustedMaxNum);
        threads.emplace_back(checkIfPrimeRange, start, end, i);
    }
}

//Task division scheme: straight division of search range (print afterwards) TODO!!
void threadByRange2(){}

//Task division scheme: threads are for divisibility testing (print immediately)
void threadByDiv1(){}

//Task division scheme: threads are for divisibility testing (print afterwards)
void threadByDiv2(){}


int main(){
    string algoStartTime;

    //algorithm start (put into selector later)
    cout << "Checking for primes up to number " << numY << " using " << threadX << " threads." << "\n";
    startTime = chrono::steady_clock::now();
    algoStartTime = getCurrentTime();
    cout << "Start time | " << algoStartTime << "\n";

    threadByRange1(threadX, numY);
    joinThreads();
    
    cout << "Start time | " << algoStartTime << "\n";
    cout << "End time   | " << getCurrentTime() << "\n";
    cout << "Total time elapsed -> " << getTimestamp() << "\n";
}