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
mutex printMutex;
mutex cvMtx;
condition_variable cv;

struct Job{
    int start;
    int end;
    int num;
    bool isReady;
};

vector<string> outputBuffer; //output buffer for printing afterwards
vector<Job> jobs; //vector holding jobs
atomic<int> divisorFound = 0;
atomic<bool> stopAllThreads = false; //flag to stop all threads
chrono::steady_clock::time_point startTime; //holds the start time, updated per run

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

void makeJobs(int threadCount, int num){
    int maxDivisor = static_cast<int>(sqrt(num));
    int rangeSize = (maxDivisor + threadCount - 1) / threadCount;

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
        }
    }
}

bool checkIfDivisible(int divNum, int targetNum){
    if(targetNum % divNum == 0){
        return true;
    }
    else return false;
}

void getJob(int id){
    while(!stopAllThreads){
        if(jobs[id].isReady){
            Job thisJob = jobs[id];

            for(int i=thisJob.start; i<thisJob.end; i++){
                if (divisorFound > 0) break;

                if(checkIfDivisible(i, thisJob.num) == true){
                    int divisorExpected = 0;
                    if(divisorFound.compare_exchange_strong(divisorExpected, i)){ //first thread to change divisorFound from 0 wins
                        string timestamp = getTimestamp();
                        lock_guard<mutex> lock(printMutex);
                        cout << timestamp << " | " << thisJob.num << " -> NOT PRIME : Thread " << id << " found " << i << "\n";
                    }

                    break;
                }
            }
            jobs[id].isReady = false;
            {
                lock_guard<mutex> lk(cvMtx);
                cv.notify_one();
            }
        }
    }
}

//Task division scheme: threads are for divisibility testing - DONE!!
void threadByDiv(int threadCount, int maxNum){ //add mode later
    vector<thread> threads;
    // int start, end;
    stopAllThreads = false;

    for(int t=0; t<threadCount; t++){
        threads.emplace_back(getJob, t);
    }

    for(int i=0; i<=maxNum; i++){
        if(i == 0 || i == 1){
            string timestamp = getTimestamp();
            cout << timestamp << " | " << i << " -> NOT PRIME" << "\n";  
        }

        else{
            divisorFound = 0;
            makeJobs(threadCount, i);


            unique_lock<mutex> lk(cvMtx);
            cv.wait(lk, [&] {
                if(divisorFound > 0) return true; //thread has printed not prime, move on
                for(int t=0; t<threadCount; t++){ //checks if all jobs are done for this num
                    if(jobs[t].isReady) return false; 
                }
                return true; //num is prime, move on
            });

            if (divisorFound == 0){
                string timestamp = getTimestamp();
                cout << timestamp << " | " << i << " -> PRIME" << "\n";
            }
        }
    }
    stopAllThreads = true;
    joinThreads(threads);
    threads.clear();
}

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
                threadByDiv(threadX, numY);
                algoEndTime = getCurrentTime();
                
                cout << "Start time | " << algoStartTime << "\n";
                cout << "End time   | " << algoEndTime << "\n";
                cout << "Total time elapsed -> " << getTimestamp() << "\n";

                break;

            case 4:

                break;

            case 5:
                exit = true;
                break;
        }
    }
}

//ISSUE: for case 3 sometimes nonprimes become prime