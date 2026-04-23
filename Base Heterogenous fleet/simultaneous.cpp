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

// --- DATA STRUCTURES ---
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
    double clock_time;
    double current_load;
    int current_node;
    vector<int> route;
    bool is_active;
    double max_capacity; // Moved to the end to match most initializers
};

struct Candidate
{
    int customer = -1;
    int vehicle_idx = -1; // -1: None, -2: New Truck, >=0: Active Fleet Index
    double travel = 1e18, arrive = 1e18, start = 1e18, finish = 1e18;
};

// --- GLOBAL CONTEXT ---
vector<Interval> traffic_profile;
vector<Customer> customers;
vector<double> garage_capacities;
double DEPOT_DUE;
int TOTAL_VEHICLES;
static const double EPS = 1e-9;

// --- UTILITY FUNCTIONS ---
double get_distance(int n1, int n2)
{
    return hypot(customers[n1].x - customers[n2].x, customers[n1].y - customers[n2].y);
}

double get_travel_time(double dist, double start_time)
{
    double dist_rem = dist;
    double curr_time = start_time;
    while (dist_rem > 0.001)
    {
        Interval ci = traffic_profile.back();
        for (auto &iv : traffic_profile)
        {
            if (curr_time >= iv.start && curr_time < iv.end)
            {
                ci = iv;
                break;
            }
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

    file >> dummy >> dummy >> out_layout;
    int num_cust;
    file >> dummy >> num_cust;
    file >> dummy >> TOTAL_VEHICLES;

    file >> dummy;
    for (int i = 0; i < TOTAL_VEHICLES; i++)
    {
        double cap;
        file >> cap;
        garage_capacities.push_back(cap);
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

// V3 Cost Function: Earliest Finish Time
bool better_v3(const Candidate &a, const Candidate &b)
{
    if (a.customer == -1)
        return false;
    if (b.customer == -1)
        return true;
    if (abs(a.finish - b.finish) > EPS)
        return a.finish < b.finish;
    return a.customer < b.customer;
}

Candidate eval_hetero_move(const Vehicle &v, int cust_id, bool is_new, double custom_cap = 0)
{
    Candidate c;
    double cap = is_new ? custom_cap : v.max_capacity;
    if (v.current_load + customers[cust_id].demand > cap + EPS)
        return c;

    double depart = is_new ? 0.0 : v.clock_time;
    int from = is_new ? 0 : v.current_node;

    c.customer = cust_id;
    c.travel = get_travel_time(get_distance(from, cust_id), depart);
    c.arrive = depart + c.travel;
    c.start = max(c.arrive, customers[cust_id].ready);
    c.finish = c.start + customers[cust_id].service;

    if (c.finish > customers[cust_id].due + EPS)
        return Candidate();
    return c;
}

// --- CORE SOLVER ---
string solve_simultaneous_hetero_single(mt19937 &rng)
{
    vector<bool> visited(customers.size(), false);
    visited[0] = true;
    int unvisited = (int)customers.size() - 1;
    vector<Vehicle> fleet;
    vector<double> garage = garage_capacities;

    while (unvisited > 0)
    {
        vector<Candidate> valid_moves;

        // 1. Check Active Fleet
        for (int v = 0; v < (int)fleet.size(); v++)
        {
            for (int i = 1; i < (int)customers.size(); i++)
            {
                if (visited[i])
                    continue;
                Candidate cand = eval_hetero_move(fleet[v], i, false);
                if (cand.customer != -1)
                {
                    cand.vehicle_idx = v;
                    valid_moves.push_back(cand);
                }
            }
        }

        // 2. Check Garage (Best-Fit Logic)
        if (!garage.empty())
        {
            for (int i = 1; i < (int)customers.size(); i++)
            {
                if (visited[i])
                    continue;
                int best_g_idx = -1;
                for (int g = 0; g < (int)garage.size(); g++)
                {
                    if (garage[g] >= customers[i].demand - EPS)
                    {
                        if (best_g_idx == -1 || garage[g] < garage[best_g_idx])
                            best_g_idx = g;
                    }
                }
                if (best_g_idx != -1)
                {
                    // FIXED: Corrected initialization to match Vehicle struct members
                    Vehicle tmp = {0, 0.0, 0.0, 0, {0}, false, garage[best_g_idx]};
                    Candidate cand = eval_hetero_move(tmp, i, true, garage[best_g_idx]);
                    if (cand.customer != -1)
                    {
                        cand.vehicle_idx = -2;
                        valid_moves.push_back(cand);
                    }
                }
            }
        }

        if (valid_moves.empty())
            return "INFEASIBLE";
        sort(valid_moves.begin(), valid_moves.end(), better_v3);

        int top_k = min(5, (int)valid_moves.size());
        uniform_int_distribution<> d(0, top_k - 1);
        Candidate best = valid_moves[d(rng)];

        if (best.vehicle_idx == -2)
        {
            int g_idx = -1;
            for (int i = 0; i < (int)garage.size(); i++)
            {
                if (garage[i] >= customers[best.customer].demand - EPS)
                {
                    if (g_idx == -1 || garage[i] < garage[g_idx])
                        g_idx = i;
                }
            }
            Vehicle nv = {(int)fleet.size() + 1, best.finish, customers[best.customer].demand, best.customer, {0, best.customer}, true, garage[g_idx]};
            fleet.push_back(nv);
            garage.erase(garage.begin() + g_idx);
        }
        else
        {
            Vehicle &v = fleet[best.vehicle_idx];
            v.current_load += customers[best.customer].demand;
            v.clock_time = best.finish;
            v.current_node = best.customer;
            v.route.push_back(best.customer);
        }
        visited[best.customer] = true;
        unvisited--;
    }

    int used = 0;
    for (auto &v : fleet)
    {
        double ret = get_travel_time(get_distance(v.current_node, 0), v.clock_time);
        if (v.clock_time + ret > DEPOT_DUE + EPS)
            return "INFEASIBLE";
        used++;
    }
    return to_string(used);
}

// --- MAIN RUNNER ---
int main()
{
    ifstream file("TD_HETERO_UNIQUE_500_Instances.txt");
    if (!file.is_open())
    {
        cout << "Dataset not found!" << endl;
        return 1;
    }

    mt19937 rng(2026);
    string layout;

    // Stats structure to track success and fleet size
    struct Stats
    {
        int total = 0, success = 0;
        double total_veh = 0;
    };
    Stats overall, clustered, random, mixed;

    while (load_next_instance(file, layout))
    {
        overall.total++;
        if (layout == "C")
            clustered.total++;
        else if (layout == "R")
            random.total++;
        else if (layout == "RC")
            mixed.total++;

        int best_in_instance = 1e9;

        // V4: Run 20 iterations to find the best global fleet matching
        for (int i = 0; i < 20; i++)
        {
            string res = solve_simultaneous_hetero_single(rng);
            if (res != "INFEASIBLE")
            {
                best_in_instance = min(best_in_instance, stoi(res));
            }
        }

        if (best_in_instance != 1e9)
        {
            // Update Overall
            overall.success++;
            overall.total_veh += best_in_instance;

            // Update specific layout
            if (layout == "C")
            {
                clustered.success++;
                clustered.total_veh += best_in_instance;
            }
            else if (layout == "R")
            {
                random.success++;
                random.total_veh += best_in_instance;
            }
            else if (layout == "RC")
            {
                mixed.success++;
                mixed.total_veh += best_in_instance;
            }
        }
    }

    auto print_stats = [](string label, Stats s)
    {
        if (s.total == 0)
            return;
        double success_rate = (s.success * 100.0 / s.total);
        double avg_veh = (s.success ? (s.total_veh / s.success) : 0);

        cout << "--- " << label << " (" << s.total << ") ---\n";
        cout << fixed << setprecision(1);
        cout << "  Success: " << success_rate << "%  Avg Veh: " << avg_veh << endl;
    };

    cout << "=======================================================\n";
    cout << "   HETEROGENEOUS SIMULTANEOUS V4 RESULTS\n";
    cout << "=======================================================\n";
    print_stats("OVERALL", overall);
    print_stats("CLUSTERED (C)", clustered);
    print_stats("RANDOM (R)", random);
    print_stats("MIXED (RC)", mixed);

    return 0;
}