#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <math.h>
#include <mutex>

using namespace std;
mutex printMutex;

vector<thread> threads; //global thread container
vector<string> outputBuffer; //output buffer for printing afterwards
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

void joinThreads(){
    for(thread &thread : threads){
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void cleanup(){
    threads.clear();
    outputBuffer.clear();
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
void threadByRange2(int threadCount, int maxNum){
    int adjustedMaxNum = maxNum + 1;

    int rangeSize = (adjustedMaxNum + threadCount - 1) / threadCount;

    for(int i=0; i<threadCount; i++){
        int start = i * rangeSize;
        int end = min(start + rangeSize, adjustedMaxNum);
        threads.emplace_back(checkIfPrimeRangeBuffer, start, end, i);
    }
}

//Task division scheme: threads are for divisibility testing (print immediately)
void threadByDiv1(){}

//Task division scheme: threads are for divisibility testing (print afterwards)
void threadByDiv2(){}


int main(){
    string algoStartTime;
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

                threadByRange1(threadX, numY);
                joinThreads();
                
                cout << "Start time | " << algoStartTime << "\n";
                cout << "End time   | " << getCurrentTime() << "\n";
                cout << "Total time elapsed -> " << getTimestamp() << "\n";

                cleanup();
                break;

            case 2:
                cout << "Checking for primes up to number " << numY << " using " << threadX << " threads." << "\n";
                startTime = chrono::steady_clock::now();
                algoStartTime = getCurrentTime();
                cout << "Start time | " << algoStartTime << "\n";

                outputBuffer.resize(numY+1);
                threadByRange2(threadX, numY);
                joinThreads();
                printBuffer();
                
                cout << "Start time | " << algoStartTime << "\n";
                cout << "End time   | " << getCurrentTime() << "\n";
                cout << "Total time elapsed -> " << getTimestamp() << "\n";

                cleanup();
                break;

            case 3:
                cout << "Checking for primes up to number " << numY << " using " << threadX << " threads." << "\n";
                startTime = chrono::steady_clock::now();
                algoStartTime = getCurrentTime();
                cout << "Start time | " << algoStartTime << "\n";

                break;

            case 4:
                cout << "Checking for primes up to number " << numY << " using " << threadX << " threads." << "\n";
                startTime = chrono::steady_clock::now();
                algoStartTime = getCurrentTime();
                cout << "Start time | " << algoStartTime << "\n";

                break;
                
            case 5:
                exit = true;
                break;
        }
    }
}