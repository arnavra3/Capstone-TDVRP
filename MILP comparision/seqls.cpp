#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <limits>
#include <random>

using namespace std;

struct Interval
{
    double start, end, speed;
};
struct Customer
{
    int id;
    double x, y, demand, ready, due, service;
};
struct Vehicle
{
    int id;
    double clock_time, current_load, max_capacity;
    int current_node;
    vector<int> route;
};
struct Candidate
{
    int customer = -1;
    double travel = 1e18, arrive = 1e18, start = 1e18, finish = 1e18;
};

// New struct to hold the final answer for printing
struct Result
{
    bool feasible = false;
    double total_time = 1e18;
    vector<Vehicle> fleet;
};

vector<Interval> traffic_profile;
vector<Customer> customers;
vector<double> garage_capacities;
string instance_name; // Added to match Python output
double DEPOT_DUE;
int TOTAL_VEHICLES;
static const double EPS = 1e-9;

// --- SHARED UTILS ---
double get_distance(int n1, int n2) { return hypot(customers[n1].x - customers[n2].x, customers[n1].y - customers[n2].y); }
double get_travel_time(double dist, double start_time)
{
    double dist_rem = dist;
    double curr_time = start_time;
    while (dist_rem > 0.001)
    {
        Interval ci = traffic_profile.back();
        for (auto &iv : traffic_profile)
            if (curr_time >= iv.start && curr_time < iv.end)
            {
                ci = iv;
                break;
            }
        double time_left = ci.end - curr_time;
        double max_dist = time_left * ci.speed;
        if (dist_rem <= max_dist + EPS)
        {
            curr_time += (dist_rem / ci.speed);
            dist_rem = 0;
        }
        else
        {
            dist_rem -= max_dist;
            curr_time = ci.end;
        }
    }
    return curr_time - start_time;
}

bool load_next_instance(ifstream &file, string &out_layout)
{
    traffic_profile.clear();
    customers.clear();
    garage_capacities.clear();
    string dummy;

    while (file >> dummy && dummy != "INSTANCE")
        ;
    if (!file)
        return false;

    file >> instance_name;
    file >> dummy >> out_layout;

    int num_cust;
    file >> dummy >> num_cust;
    file >> dummy >> TOTAL_VEHICLES;

    file >> dummy;
    for (int i = 0; i < TOTAL_VEHICLES; i++)
    {
        double c;
        file >> c;
        garage_capacities.push_back(c);
    }

    int m;
    file >> dummy >> m;
    file >> dummy >> dummy >> dummy;
    for (int i = 0; i < m; i++)
    {
        Interval iv;
        file >> iv.start >> iv.end >> iv.speed;
        traffic_profile.push_back(iv);
    }

    file >> dummy;
    for (int i = 0; i < 7; i++)
        file >> dummy;

    for (int i = 0; i <= num_cust; i++)
    {
        Customer c;
        file >> c.id >> c.x >> c.y >> c.demand >> c.ready >> c.due >> c.service;
        customers.push_back(c);
    }
    DEPOT_DUE = customers[0].due;
    return true;
}

Candidate eval_move(const Vehicle &v, int cust_id)
{
    if (v.current_load + customers[cust_id].demand > v.max_capacity + EPS)
        return Candidate();
    Candidate c;
    c.customer = cust_id;
    c.travel = get_travel_time(get_distance(v.current_node, cust_id), v.clock_time);
    c.arrive = v.clock_time + c.travel;
    c.start = max(c.arrive, customers[cust_id].ready);
    c.finish = c.start + customers[cust_id].service;
    if (c.finish > customers[cust_id].due + EPS)
        return Candidate();
    return c;
}

bool better_v1(const Candidate &a, const Candidate &b) { return a.travel + EPS < b.travel; }
bool better_v2(const Candidate &a, const Candidate &b) { return a.start + EPS < b.start; }
bool better_v3(const Candidate &a, const Candidate &b) { return a.finish + EPS < b.finish; }

Result solve_seqls(int version, mt19937 &rng)
{
    Result res;
    vector<bool> visited(customers.size(), false);
    visited[0] = true;
    int unvisited = (int)customers.size() - 1;

    vector<double> local_garage = garage_capacities;
    sort(local_garage.rbegin(), local_garage.rend()); // LARGE TO SMALL

    int used = 0;
    res.total_time = 0.0; // Track sum of return times

    while (unvisited > 0)
    {
        if (used >= TOTAL_VEHICLES)
            return Result{false}; // INFEASIBLE

        // Initialize vehicle. Node 0 is pushed into the route automatically.
        Vehicle truck = {used + 1, 0.0, 0.0, local_garage[used], 0, {0}};
        bool added_any = false;

        while (true)
        {
            vector<Candidate> cands;
            for (int i = 1; i < (int)customers.size(); i++)
            {
                if (visited[i])
                    continue;
                Candidate cand = eval_move(truck, i);
                if (cand.customer != -1)
                    cands.push_back(cand);
            }
            if (cands.empty())
                break;

            Candidate best;
            if (version == 4)
            {
                sort(cands.begin(), cands.end(), better_v3);
                int k = min(5, (int)cands.size());
                uniform_int_distribution<> d(0, k - 1);
                best = cands[d(rng)];
            }
            else
            {
                auto comp = (version == 1) ? better_v1 : (version == 2) ? better_v2
                                                                        : better_v3;
                sort(cands.begin(), cands.end(), comp);
                best = cands[0];
            }

            truck.current_load += customers[best.customer].demand;
            truck.clock_time = best.finish;
            truck.current_node = best.customer;
            truck.route.push_back(best.customer); // <-- Record path

            visited[best.customer] = true;
            unvisited--;
            added_any = true;
        }

        if (!added_any)
            return Result{false}; // Dead end

        // Return to depot logic
        double ret = get_travel_time(get_distance(truck.current_node, 0), truck.clock_time);
        if (truck.clock_time + ret > DEPOT_DUE + EPS)
            return Result{false};

        truck.clock_time += ret;
        truck.route.push_back(0); // Arrive back at depot

        res.total_time += truck.clock_time; // Sum up total system time
        res.fleet.push_back(truck);
        used++;
    }

    res.feasible = true;
    return res;
}

// Helper function to print results neatly
void print_version_result(int version, const Result& res) {
    cout << "--- Version " << version << " ---\n";
    if (res.feasible) {
        cout << fixed << setprecision(2);
        cout << "SUCCESS! Route Total Time: " << res.total_time << "\n";
        for (const auto &v : res.fleet) {
            cout << "  Vehicle " << v.id << " (Cap " << (int)v.max_capacity << "): ";
            for (size_t i = 0; i < v.route.size(); i++) {
                cout << v.route[i];
                if (i < v.route.size() - 1)
                    cout << " -> ";
            }
            cout << " [Arrived back at " << v.clock_time << " mins]\n";
        }
    } else {
        cout << "NO SOLUTION FOUND.\n";
    }
}

int main()
{
    // Pointed to the new micro-instance file for MILP comparison
    ifstream file("TD_MICRO_10_Instances.txt");
    if (!file)
    {
        cout << "Could not open file.\n";
        return 1;
    }

    mt19937 rng(2026);
    string layout;

    while (load_next_instance(file, layout))
    {
        cout << "\n==================================================\n";
        cout << "SOLVING INSTANCE: " << instance_name << " (" << customers.size() - 1 << " Cust, " << TOTAL_VEHICLES << " Veh)\n";

        // Version 1
        Result r1 = solve_seqls(1, rng);
        print_version_result(1, r1);

        // Version 2
        Result r2 = solve_seqls(2, rng);
        print_version_result(2, r2);

        // Version 3
        Result r3 = solve_seqls(3, rng);
        print_version_result(3, r3);

        // Version 4 (Run 20 times and pick the best)
        Result best_r4;
        for (int i = 0; i < 20; i++)
        {
            Result r = solve_seqls(4, rng);
            if (r.feasible && r.total_time < best_r4.total_time)
            {
                best_r4 = r;
            }
        }
        print_version_result(4, best_r4);
    }

    return 0;
}