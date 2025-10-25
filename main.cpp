#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <math.h>
#include <cmath>
#include <mutex>
#include <queue>
#include <functional>
#include <condition_variable>

using namespace std;

struct Job{
    int start;
    int end;
    int num;
    bool isReady;
};

mutex printMutex;
mutex jobDoneMutex;
mutex jobReadyMutex;

condition_variable jobDoneCV;
condition_variable jobReadyCV;

vector<string> outputBuffer; //output buffer for printing afterwards
chrono::steady_clock::time_point startTime; //holds the start time, updated per run

vector<Job> jobs;
atomic<bool> stopAllThreads = false; //flag to stop all threads
atomic<int> divisorFoundThreadID = -1; //holds the thread id that found the divisor, -1 if none
atomic<int> divisorFound = -1; //holds the found divisor, -1 if none
atomic<int> pendingJobs = 0; //holds the number of jobs still running

string getTimestamp(){ //time from startTime
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

string getCurrentTime(){ //system time right now
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

void printBuffer(){
    for(auto &log : outputBuffer){
        cout << log << "\n";
    }
}

void joinThreads(vector<thread>& threads){
    for(thread &thread : threads){
        if (thread.joinable()) {
            thread.join();
        }
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

void checkIfPrimeRangeBuffer(int start, int end, int id){
    for(int num=start; num<end; num++){
        if(checkIfPrime(num) == true){
            string timestamp = getTimestamp();
            ostringstream logString;

            logString << timestamp << " Thread " << id << " | " << num << " -> PRIME" << "\n";
            outputBuffer[num] = logString.str();
        }
        else {
            string timestamp = getTimestamp();
            ostringstream logString;

            logString << timestamp << " Thread " << id << " | " << num << " -> NOT PRIME" << "\n";
            outputBuffer[num] = logString.str();
        }
    }
}

//Task division scheme: straight division of search range - DONE!!
void threadByRange(int threadCount, int maxNum, string mode = "print"){
    vector<thread> threads;
    int adjustedMaxNum = maxNum + 1;

    int rangeSize = (adjustedMaxNum + threadCount - 1) / threadCount;

    for(int i=0; i<threadCount; i++){
        int start = i * rangeSize;
        int end = min(start + rangeSize, adjustedMaxNum);

        if(mode == "print"){
            threads.emplace_back(checkIfPrimeRange, start, end, i);
        }

        else if (mode == "log"){
            threads.emplace_back(checkIfPrimeRangeBuffer, start, end, i);
        }
    }

    joinThreads(threads);
    threads.clear();
}

//--------------------------------------------------------------------

bool checkIfDivisible(int divNum, int targetNum){
    if(targetNum % divNum == 0){
        return true;
    }
    else return false;
}

void getJob(int id){
    while(true){
        unique_lock<mutex> lock(jobReadyMutex);
        jobReadyCV.wait(lock, [id]() { return stopAllThreads.load() || jobs[id].isReady; }); //wait for makeJobs to finish
        lock.unlock();

        if(stopAllThreads.load()) break;

        if(jobs[id].isReady){
            jobs[id].isReady = false;
            Job job = jobs[id];
            
            for(int i=job.start; i<job.end && divisorFound.load() == -1; i++){
                if(checkIfDivisible(i, job.num)){
                    divisorFound.store(i);
                    divisorFoundThreadID.store(id);
                    break;
                }
            }

            pendingJobs.fetch_sub(1);

            if(pendingJobs.load() == 0){
                lock_guard<mutex> lock(jobDoneMutex);
                jobDoneCV.notify_all();
            }
        }
    }
}

void makeJobs(int threadCount, int num){
    pendingJobs = 0;
    int maxDivisor = static_cast<int>(sqrt(num));
    int rangeSize = (maxDivisor + threadCount - 1) / threadCount;
    {
        lock_guard<mutex> lock(jobReadyMutex);
    
        for(int i=0; i<threadCount; i++){
            jobs[i].isReady = false;
            jobs[i].start = 0;
            jobs[i].end = 0;
            jobs[i].num = 0;
        }

        for(int i=0; i<threadCount; i++){
            int start = i * rangeSize + 2;
            int end = min(start + rangeSize, maxDivisor + 1);

            if(start >= end){ // thread has no work, dont activate its job
                jobs[i].isReady = false;
            }

            else{
                jobs[i].start = start;
                jobs[i].end = end;
                jobs[i].num = num;
                jobs[i].isReady = true;
                pendingJobs++;
            }
        }
        
        jobReadyCV.notify_all();
    }
}

void threadByDiv(int threadCount, int maxNum, string mode="print"){
    vector<thread> threads;
    stopAllThreads = false;

    for(int t=0; t<threadCount; t++){
        threads.emplace_back(getJob, t);
    }

    for(int i=0; i<=maxNum; i++){
        divisorFound = -1;
        divisorFoundThreadID = -1;

        if(i == 0 || i == 1){
            if(mode == "print"){
                string timestamp = getTimestamp();
                cout << timestamp << " | " << i << " -> NOT PRIME" << "\n";  
            }

            else if(mode == "log"){
                string timestamp = getTimestamp();
                ostringstream logString;

                logString << timestamp << " | " << i << " -> NOT PRIME" << "\n";
                outputBuffer[i] = logString.str();
            }
        }

        else{
            makeJobs(threadCount, i);

            {   
                unique_lock<mutex> lock(jobDoneMutex);
                // cout << "Waiting..." << "\n";
                jobDoneCV.wait(lock, []() -> bool { return pendingJobs.load() <= 0; }); //trying <=, is usually ==
                // cout << "Woke up!" << "\n";
            }
            
            if(divisorFound.load() == -1){
                if(mode == "print"){
                    string timestamp = getTimestamp();
                    cout << timestamp << " | " << i << " -> PRIME" << "\n";
                }

                else if(mode == "log"){
                    string timestamp = getTimestamp();
                    ostringstream logString;

                    logString << timestamp << " | " << i << " -> PRIME" << "\n";
                    outputBuffer[i] = logString.str();
                }
                
            }

            else{
                if(mode == "print"){
                    string timestamp = getTimestamp();
                    lock_guard<mutex> lock(printMutex);
                    cout << timestamp << " | " << i << " -> NOT PRIME : Thread " << divisorFoundThreadID << " found " << divisorFound<< "\n";
                }

                else if(mode == "log"){
                    string timestamp = getTimestamp();
                    ostringstream logString;

                    logString << timestamp << " | " << i << " -> NOT PRIME: Thread " << divisorFoundThreadID << " found " << divisorFound<< "\n";
                    outputBuffer[i] = logString.str();
                }
                
            }
        }
    }

    stopAllThreads = true;
    {
    lock_guard<mutex> lock(jobReadyMutex);
    jobReadyCV.notify_all();
    }
    joinThreads(threads);
    threads.clear();
}

//--------------------------------------------------------------------

int main(){
    string algoStartTime, algoEndTime, timestamp;
    int threadX, numY;

    // menu stuff
    bool exit = false;
    int choice;

    while(true) {
            cout << "Enter the number of threads: ";
            if(cin >> threadX && threadX > 0) {
                break;
            } else {
                cout << "Invalid input. Please enter a positive integer.\n";
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
            }
        }

    while(true) {
        cout << "Enter the maximum number to check: ";
        if(cin >> numY && numY > 0) {
            break;
        } else {
            cout << "Invalid input. Please enter a positive integer.\n";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }
    }

    while(!exit){
        cout << "\n=============== MENU ===============\n";
        cout << "Threads: " << threadX << " | " << "Max number: " << numY << "\n";
        cout << "1. Divide by search range (print immediately)\n";
        cout << "2. Divide by search range (print afterwards)\n";
        cout << "3. Divide by divisibility (print immediately)\n";
        cout << "4. Divide by divisibility (print afterwards)\n";
        cout << "5. Exit\n";
        cout << "Select an option (1-5): ";

        if (!(cin >> choice)) {
        cout << "Invalid input! Please enter a number.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        continue;
        }

        switch(choice) {
            case 1:
                cout << "Checking for primes up to number " << numY << " using " << threadX << " threads." << "\n";
                startTime = chrono::steady_clock::now();
                algoStartTime = getCurrentTime();
                cout << "Start time | " << algoStartTime << "\n";

                threadByRange(threadX, numY, "print");
                algoEndTime = getCurrentTime();
                
                cout << "Start time | " << algoStartTime << "\n";
                cout << "End time   | " << algoEndTime << "\n";
                cout << "Total time elapsed -> " << getTimestamp() << "\n";

                break;

            case 2:
                cout << "Checking for primes up to number " << numY << " using " << threadX << " threads." << "\n";
                startTime = chrono::steady_clock::now();
                algoStartTime = getCurrentTime();
                cout << "Start time | " << algoStartTime << "\n";

                outputBuffer.resize(numY+1);
                threadByRange(threadX, numY, "log");
                algoEndTime = getCurrentTime();
                timestamp = getTimestamp();
                printBuffer();
                
                cout << "Start time | " << algoStartTime << "\n";
                cout << "End time   | " << algoEndTime << "\n";
                cout << "Total time elapsed (not including log printing) -> " << timestamp << "\n";

                outputBuffer.clear();

                break;

            case 3:
                cout << "Checking for primes up to number " << numY << " using " << threadX << " threads." << "\n";
                startTime = chrono::steady_clock::now();
                algoStartTime = getCurrentTime();
                cout << "Start time | " << algoStartTime << "\n";

                jobs.resize(threadX);
                threadByDiv(threadX, numY, "print");
                algoEndTime = getCurrentTime();
                
                cout << "Start time | " << algoStartTime << "\n";
                cout << "End time   | " << algoEndTime << "\n";
                cout << "Total time elapsed -> " << getTimestamp() << "\n";

                break;

            case 4:
                cout << "Checking for primes up to number " << numY << " using " << threadX << " threads." << "\n";
                startTime = chrono::steady_clock::now();
                algoStartTime = getCurrentTime();
                cout << "Start time | " << algoStartTime << "\n";

                outputBuffer.resize(numY+1);
                jobs.resize(threadX);
                threadByDiv(threadX, numY, "log");
                algoEndTime = getCurrentTime();
                timestamp = getTimestamp();
                printBuffer();

                cout << "Start time | " << algoStartTime << "\n";
                cout << "End time   | " << algoEndTime << "\n";
                cout << "Total time elapsed (not including log printing) -> " << timestamp << "\n";

                outputBuffer.clear();

                break;

            case 5:
                exit = true;
                break;
        }
    }
}

