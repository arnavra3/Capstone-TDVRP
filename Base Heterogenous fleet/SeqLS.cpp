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

vector<Interval> traffic_profile;
vector<Customer> customers;
vector<double> garage_capacities;
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
    file >> dummy >> dummy >> out_layout;
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

// =======================================================
// VERSION-SPECIFIC SORTING (V1, V2, V3)
// =======================================================
bool better_v1(const Candidate &a, const Candidate &b) { return a.travel + EPS < b.travel; }
bool better_v2(const Candidate &a, const Candidate &b) { return a.start + EPS < b.start; }
bool better_v3(const Candidate &a, const Candidate &b) { return a.finish + EPS < b.finish; }

string solve_seqls(int version, mt19937 &rng)
{
    vector<bool> visited(customers.size(), false);
    visited[0] = true;
    int unvisited = (int)customers.size() - 1;
    vector<double> local_garage = garage_capacities;
    sort(local_garage.rbegin(), local_garage.rend()); // LARGE TO SMALL
    int used = 0;

    while (unvisited > 0)
    {
        if (used >= TOTAL_VEHICLES)
            return "INFEASIBLE";
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

            if (version == 4)
            {
                sort(cands.begin(), cands.end(), better_v3);
                int k = min(5, (int)cands.size());
                uniform_int_distribution<> d(0, k - 1);
                Candidate best = cands[d(rng)];
                truck.current_load += customers[best.customer].demand;
                truck.clock_time = best.finish;
                truck.current_node = best.customer;
                visited[best.customer] = true;
                unvisited--;
                added_any = true;
            }
            else
            {
                auto comp = (version == 1) ? better_v1 : (version == 2) ? better_v2
                                                                        : better_v3;
                sort(cands.begin(), cands.end(), comp);
                Candidate best = cands[0];
                truck.current_load += customers[best.customer].demand;
                truck.clock_time = best.finish;
                truck.current_node = best.customer;
                visited[best.customer] = true;
                unvisited--;
                added_any = true;
            }
        }
        if (!added_any)
            return "INFEASIBLE";
        double ret = get_travel_time(get_distance(truck.current_node, 0), truck.clock_time);
        if (truck.clock_time + ret > DEPOT_DUE + EPS)
            return "INFEASIBLE";
        used++;
    }
    return to_string(used);
}

int main()
{
    ifstream file("TD_HETERO_UNIQUE_500_Instances.txt");
    if (!file)
        return 1;

    mt19937 rng(2026);
    string layout;

    // Structure to hold results for each version across layouts
    struct Stats
    {
        int total = 0, v1 = 0, v2 = 0, v3 = 0, v4 = 0;
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

        // Run Versions
        bool s1 = (solve_seqls(1, rng) != "INFEASIBLE");
        bool s2 = (solve_seqls(2, rng) != "INFEASIBLE");
        bool s3 = (solve_seqls(3, rng) != "INFEASIBLE");

        int b4 = 1e9;
        for (int i = 0; i < 20; i++)
        {
            string r = solve_seqls(4, rng);
            if (r != "INFEASIBLE")
                b4 = min(b4, stoi(r));
        }
        bool s4 = (b4 != 1e9);

        // Record Overall
        if (s1)
            overall.v1++;
        if (s2)
            overall.v2++;
        if (s3)
            overall.v3++;
        if (s4)
            overall.v4++;

        // Record by Layout
        if (layout == "C")
        {
            if (s1)
                clustered.v1++;
            if (s2)
                clustered.v2++;
            if (s3)
                clustered.v3++;
            if (s4)
                clustered.v4++;
        }
        else if (layout == "R")
        {
            if (s1)
                random.v1++;
            if (s2)
                random.v2++;
            if (s3)
                random.v3++;
            if (s4)
                random.v4++;
        }
        else if (layout == "RC")
        {
            if (s1)
                mixed.v1++;
            if (s2)
                mixed.v2++;
            if (s3)
                mixed.v3++;
            if (s4)
                mixed.v4++;
        }
    }

    auto print_row = [](string label, Stats s)
    {
        cout << "--- " << label << " (" << s.total << ") ---\n";
        cout << fixed << setprecision(1);
        cout << "  V1: " << s.v1 * 100.0 / s.total << "%  V2: " << s.v2 * 100.0 / s.total
             << "%  V3: " << s.v3 * 100.0 / s.total << "%  V4: " << s.v4 * 100.0 / s.total << "%\n";
    };

    cout << "=======================================================\n";
    cout << "   SEQLS (HETEROGENEOUS LARGE-TO-SMALL) RESULTS\n";
    cout << "=======================================================\n";
    print_row("OVERALL", overall);
    print_row("CLUSTERED (C)", clustered);
    print_row("RANDOM (R)", random);
    print_row("MIXED (RC)", mixed);

    return 0;
}