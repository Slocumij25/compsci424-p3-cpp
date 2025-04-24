#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <sstream>
#include <random>
#include <chrono>

using namespace std;

int num_resources, num_processes;
vector<int> Available;
vector<vector<int>> Max;
vector<vector<int>> Allocation;
vector<vector<int>> Need;
mutex mtx;

bool is_safe() {
    vector<int> work = Available;
    vector<bool> finish(num_processes, false);

    while (true) {
        bool progress = false;
        for (int i = 0; i < num_processes; ++i) {
            if (!finish[i]) {
                bool can_finish = true;
                for (int j = 0; j < num_resources; ++j) {
                    if (Need[i][j] > work[j]) {
                        can_finish = false;
                        break;
                    }
                }
                if (can_finish) {
                    for (int j = 0; j < num_resources; ++j)
                        work[j] += Allocation[i][j];
                    finish[i] = true;
                    progress = true;
                }
            }
        }
        if (!progress) break;
    }

    return all_of(finish.begin(), finish.end(), [](bool done) { return done; });
}

bool request_resources(int pid, vector<int> req) {
    lock_guard<mutex> lock(mtx);
    cout << "Process " << pid << " requests ";
    for (int i = 0; i < num_resources; ++i) cout << req[i] << " ";

    for (int i = 0; i < num_resources; ++i) {
        if (req[i] > Need[pid][i] || req[i] > Available[i]) {
            cout << ": denied (invalid request)\n";
            return false;
        }
    }
    for (int i = 0; i < num_resources; ++i) {
        Available[i] -= req[i];
        Allocation[pid][i] += req[i];
        Need[pid][i] -= req[i];
    }
    if (is_safe()) {
        cout << ": granted\n";
        return true;
    }
    else {
        for (int i = 0; i < num_resources; ++i) {
            Available[i] += req[i];
            Allocation[pid][i] -= req[i];
            Need[pid][i] += req[i];
        }
        cout << ": denied (unsafe state)\n";
        return false;
    }
}

bool release_resources(int pid, vector<int> rel) {
    lock_guard<mutex> lock(mtx);
    cout << "Process " << pid << " releases ";
    for (int i = 0; i < num_resources; ++i) cout << rel[i] << " ";

    for (int i = 0; i < num_resources; ++i) {
        if (rel[i] > Allocation[pid][i]) {
            cout << ": denied (invalid release)\n";
            return false;
        }
    }
    for (int i = 0; i < num_resources; ++i) {
        Available[i] += rel[i];
        Allocation[pid][i] -= rel[i];
        Need[pid][i] += rel[i];
    }
    cout << ": done\n";
    return true;
}

void auto_process(int pid) {
    default_random_engine gen(chrono::system_clock::now().time_since_epoch().count() + pid);
    uniform_int_distribution<int> dist(0, 3);
    for (int i = 0; i < 3; ++i) {
        vector<int> req(num_resources), rel(num_resources);
        for (int j = 0; j < num_resources; ++j)
            req[j] = min(dist(gen), Need[pid][j]);
        request_resources(pid, req);

        this_thread::sleep_for(chrono::milliseconds(100));

        for (int j = 0; j < num_resources; ++j)
            rel[j] = min(dist(gen), Allocation[pid][j]);
        release_resources(pid, rel);

        this_thread::sleep_for(chrono::milliseconds(100));
    }
}

void run_manual_mode() {
    string command;
    while (true) {
        cout << "Enter command: ";
        getline(cin, command);
        if (command == "end") break;

        stringstream ss(command);
        string type;
        int amount, res, pid;
        ss >> type >> amount >> command >> res >> command >> pid;
        vector<int> vec(num_resources, 0);
        vec[res] = amount;
        if (type == "request")
            request_resources(pid, vec);
        else if (type == "release")
            release_resources(pid, vec);
    }
}

void read_setup(const string& filename) {
    ifstream in(filename);
    string dummy;

    in >> num_resources >> dummy;
    in >> num_processes >> dummy;

    in >> dummy;
    Available.resize(num_resources);
    for (int& x : Available) in >> x;

    in >> dummy;
    Max.assign(num_processes, vector<int>(num_resources));
    for (auto& row : Max) for (int& x : row) in >> x;

    in >> dummy;
    Allocation.assign(num_processes, vector<int>(num_resources));
    for (auto& row : Allocation) for (int& x : row) in >> x;

    Need = Max;
    for (int i = 0; i < num_processes; ++i)
        for (int j = 0; j < num_resources; ++j)
            Need[i][j] -= Allocation[i][j];
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <manual|auto> <setupfile>\n";
        return 1;
    }

    string mode = argv[1];
    read_setup(argv[2]);

    if (!is_safe()) {
        cerr << "Initial state is not safe. Exiting.\n";
        return 1;
    }

    if (mode == "manual") {
        run_manual_mode();
    }
    else if (mode == "auto") {
        vector<thread> threads;
        for (int i = 0; i < num_processes; ++i)
            threads.emplace_back(auto_process, i);
        for (auto& t : threads) t.join();
    }
    else {
        cerr << "Unknown mode: " << mode << endl;
        return 1;
    }

    return 0;
}
