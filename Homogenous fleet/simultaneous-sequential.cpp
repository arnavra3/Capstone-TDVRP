#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <limits>


// ACCORDING TO THE PAPER EARLIEST TRAVEL TIME TO REACH NODE

using namespace std;

struct Interval
{
    double start, end, speed;
};
struct Customer
{
    int id;
    double x, y, demand, ready, due, service, capacity;
};
struct Vehicle
{
    int id;
    double clock_time, current_load;
    int current_node;
    vector<int> route;
};

struct Candidate
{
    int customer = -1;
    int vehicle_idx = -1;
    double travel = 1e18;
    double arrive = 1e18;
    double start = 1e18;
    double finish = 1e18;
};

vector<Interval> traffic_profile;
vector<Customer> customers;
double DEPOT_DUE;
double VEH_CAP;
int MAX_VEHICLES;

static const double EPS = 1e-9;

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
    string dummy;
    while (file >> dummy && dummy != "INSTANCE")
        ;
    if (!file)
        return false;

    string name;
    file >> name >> dummy >> out_layout;
    int num_cust;
    file >> dummy >> num_cust;
    file >> dummy >> MAX_VEHICLES;
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
    for (int i = 0; i < 8; i++)
        file >> dummy;
    for (int i = 0; i <= num_cust; i++)
    {
        Customer c;
        file >> c.id >> c.x >> c.y >> c.demand >> c.ready >> c.due >> c.service >> c.capacity;
        customers.push_back(c);
    }
    DEPOT_DUE = customers[0].due;
    VEH_CAP = customers[0].capacity;
    return true;
}

// =======================================================
// DERIVED V1 LOGIC: Strictly Minimizes Travel Time
// =======================================================
bool better_candidate(const Candidate &a, const Candidate &b)
{
    if (a.customer == -1)
        return false;
    if (b.customer == -1)
        return true;

    // 1. PRIMARY COST FUNCTION: Smallest Travel Time (The V1 Flaw!)
    // This makes the algorithm blind to waiting time.
    if (a.travel + EPS < b.travel)
        return true;
    if (b.travel + EPS < a.travel)
        return false;

    // 2. SECONDARY: Tie-breaker! Prefer the ACTIVE truck to prevent infinite spawning.
    if (a.vehicle_idx != -2 && b.vehicle_idx == -2)
        return true;
    if (a.vehicle_idx == -2 && b.vehicle_idx != -2)
        return false;

    // 3. TERTIARY: If travel time is perfectly identical, break the tie safely.
    if (a.start + EPS < b.start)
        return true;
    if (b.start + EPS < a.start)
        return false;

    return a.customer < b.customer;
}

Candidate eval_move(const Vehicle &v, int cust_id, bool new_truck)
{
    Candidate c;
    c.customer = cust_id;
    c.vehicle_idx = new_truck ? -2 : v.id;

    double depart_time = new_truck ? 0.0 : v.clock_time;
    int from_node = new_truck ? 0 : v.current_node;
    double load = new_truck ? 0.0 : v.current_load;

    if (load + customers[cust_id].demand > VEH_CAP + EPS)
        return Candidate();

    c.travel = get_travel_time(get_distance(from_node, cust_id), depart_time);
    c.arrive = depart_time + c.travel;
    c.start = max(c.arrive, customers[cust_id].ready);
    c.finish = c.start + customers[cust_id].service;

    if (c.finish > customers[cust_id].due + EPS)
        return Candidate();
    return c;
}

string solve_sequential()
{
    vector<bool> visited(customers.size(), false);
    visited[0] = true;
    int unvisited = (int)customers.size() - 1;
    int v_count = 0;

    while (unvisited > 0)
    {
        if (v_count >= MAX_VEHICLES)
            return "INFEASIBLE";

        Vehicle truck = {++v_count, 0.0, 0.0, 0, {0}};
        bool added = false;

        while (true)
        {
            Candidate best;
            for (int i = 1; i < (int)customers.size(); i++)
            {
                if (visited[i])
                    continue;
                Candidate cand = eval_move(truck, i, false);
                if (better_candidate(cand, best))
                    best = cand;
            }

            if (best.customer == -1)
                break;

            truck.current_load += customers[best.customer].demand;
            truck.clock_time = best.finish;
            truck.current_node = best.customer;
            truck.route.push_back(best.customer);
            visited[best.customer] = true;
            unvisited--;
            added = true;
        }

        if (!added)
            return "INFEASIBLE";
        double ret = get_travel_time(get_distance(truck.current_node, 0), truck.clock_time);
        if (truck.clock_time + ret > DEPOT_DUE + EPS)
            return "INFEASIBLE";
    }
    return to_string(v_count);
}

string solve_simultaneous()
{
    vector<bool> visited(customers.size(), false);
    visited[0] = true;
    int unvisited = (int)customers.size() - 1;
    vector<Vehicle> fleet;
    fleet.push_back({1, 0.0, 0.0, 0, {0}});

    while (unvisited > 0)
    {
        Candidate best;

        for (int v = 0; v < (int)fleet.size(); v++)
        {
            for (int i = 1; i < (int)customers.size(); i++)
            {
                if (visited[i])
                    continue;
                Candidate cand = eval_move(fleet[v], i, false);
                cand.vehicle_idx = v;
                if (better_candidate(cand, best))
                    best = cand;
            }
        }

        if ((int)fleet.size() < MAX_VEHICLES)
        {
            for (int i = 1; i < (int)customers.size(); i++)
            {
                if (visited[i])
                    continue;
                Vehicle depot_like = {0, 0.0, 0.0, 0, {0}};
                Candidate cand = eval_move(depot_like, i, true);
                if (better_candidate(cand, best))
                    best = cand;
            }
        }

        if (best.customer == -1)
            return "INFEASIBLE";

        if (best.vehicle_idx == -2)
        {
            Vehicle nv;
            nv.id = (int)fleet.size() + 1;
            nv.clock_time = best.finish;
            nv.current_load = customers[best.customer].demand;
            nv.current_node = best.customer;
            nv.route = {0, best.customer};
            fleet.push_back(nv);
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
        if (v.route.size() > 1)
        {
            double ret = get_travel_time(get_distance(v.current_node, 0), v.clock_time);
            if (v.clock_time + ret > DEPOT_DUE + EPS)
                return "INFEASIBLE";
            used++;
        }
    }
    return to_string(used);
}

struct Metrics
{
    int total = 0, seq_s = 0, sim_s = 0;
    double seq_v = 0, sim_v = 0;
};

int main()
{
    ifstream file("TD_HOMO_500_Instances.txt");
    if (!file.is_open())
    {
        cout << "Generate data first!" << endl;
        return 1;
    }
    string layout;
    Metrics o, c, r, rc;

    while (load_next_instance(file, layout))
    {
        o.total++;
        Metrics *cur = (layout == "C") ? &c : (layout == "R") ? &r
                                                              : &rc;
        cur->total++;

        string res1 = solve_sequential();
        string res2 = solve_simultaneous();

        if (res1 != "INFEASIBLE")
        {
            int v = stoi(res1);
            o.seq_s++;
            o.seq_v += v;
            cur->seq_s++;
            cur->seq_v += v;
        }
        if (res2 != "INFEASIBLE")
        {
            int v = stoi(res2);
            o.sim_s++;
            o.sim_v += v;
            cur->sim_s++;
            cur->sim_v += v;
        }
    }

    auto p = [](string n, Metrics m)
    {
        cout << "--- " << n << " (" << m.total << ") ---\n";
        cout << "  [Seq] Success: " << fixed << setprecision(1) << (m.total ? (m.seq_s * 100.0 / m.total) : 0)
             << "% \tAvg Veh: " << (m.seq_s ? m.seq_v / m.seq_s : 0) << endl;
        cout << "  [Sim] Success: " << (m.total ? (m.sim_s * 100.0 / m.total) : 0)
             << "% \tAvg Veh: " << (m.sim_s ? m.sim_v / m.sim_s : 0) << "\n\n";
    };

    cout << "=======================================================\n";
    cout << "  DERIVED V1 BASELINE (TRAVEL TIME ONLY)\n";
    cout << "=======================================================\n";
    p("OVERALL", o);
    p("CLUSTERED (C)", c);
    p("RANDOM (R)", r);
    p("MIXED (RC)", rc);
    return 0;
}