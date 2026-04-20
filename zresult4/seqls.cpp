#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <limits>
#include <random>
#include <map>
#include <sstream>

using namespace std;

struct Interval { double start, end, speed; };
struct Customer { int id; double x, y, demand, ready, due, service; };
struct Vehicle { int id; double clock_time, current_load, max_capacity; int current_node; vector<int> route; };
struct Candidate { int customer = -1; double travel = 1e18, arrive = 1e18, start = 1e18, finish = 1e18; };
struct Result { bool feasible = false; double total_time = 1e18; vector<Vehicle> fleet; };

vector<Interval> traffic_profile;
vector<Customer> customers;
vector<double> garage_capacities;
string instance_name;
double DEPOT_DUE;
int TOTAL_VEHICLES;
static const double EPS = 1e-9;

double get_distance(int n1, int n2) { return hypot(customers[n1].x - customers[n2].x, customers[n1].y - customers[n2].y); }

// --- STRICT MILP MATCH: Discrete speeds with inclusive boundaries ---
double get_travel_time(double dist, double start_time) {
    double speed = traffic_profile.back().speed; 
    for (const auto &iv : traffic_profile) {
        // <= iv.end perfectly matches the MILP's inclusive constraint
        if (start_time >= iv.start && start_time <= iv.end) {
            speed = iv.speed;
            break;
        }
    }
    return dist / speed;
}

bool load_next_instance(ifstream &file, string &out_layout) {
    traffic_profile.clear(); customers.clear(); garage_capacities.clear();
    string dummy;
    while (file >> dummy && dummy != "INSTANCE");
    if (!file) return false;
    
    file >> instance_name;
    file >> dummy >> out_layout;
    
    int num_cust;
    file >> dummy >> num_cust;
    file >> dummy >> TOTAL_VEHICLES;
    
    file >> dummy;
    for (int i = 0; i < TOTAL_VEHICLES; i++) { double c; file >> c; garage_capacities.push_back(c); }
    
    int m;
    file >> dummy >> m;
    file >> dummy >> dummy >> dummy;
    for (int i = 0; i < m; i++) { Interval iv; file >> iv.start >> iv.end >> iv.speed; traffic_profile.push_back(iv); }
    
    file >> dummy; for (int i = 0; i < 7; i++) file >> dummy;
    for (int i = 0; i <= num_cust; i++) {
        Customer c; file >> c.id >> c.x >> c.y >> c.demand >> c.ready >> c.due >> c.service;
        customers.push_back(c);
    }
    DEPOT_DUE = customers[0].due;
    return true;
}

Candidate eval_move(const Vehicle &v, int cust_id) {
    if (v.current_load + customers[cust_id].demand > v.max_capacity + EPS) return Candidate();
    Candidate c;
    c.customer = cust_id;
    c.travel = get_travel_time(get_distance(v.current_node, cust_id), v.clock_time);
    c.arrive = v.clock_time + c.travel;
    c.start = max(c.arrive, customers[cust_id].ready);
    c.finish = c.start + customers[cust_id].service;
    if (c.finish > customers[cust_id].due + EPS) return Candidate();
    return c;
}

bool better_v1(const Candidate &a, const Candidate &b) { return a.travel + EPS < b.travel; }
bool better_v2(const Candidate &a, const Candidate &b) { return a.start + EPS < b.start; }
bool better_v3(const Candidate &a, const Candidate &b) { return a.finish + EPS < b.finish; }

Result solve_seqls(int version, mt19937 &rng) {
    Result res;
    vector<bool> visited(customers.size(), false);
    visited[0] = true;
    int unvisited = (int)customers.size() - 1;
    
    vector<double> local_garage = garage_capacities;
    sort(local_garage.rbegin(), local_garage.rend()); // LARGE TO SMALL (SEQ-LS)
    
    int used = 0;
    res.total_time = 0.0;
    
    while (unvisited > 0) {
        if (used >= TOTAL_VEHICLES) return Result{false};
        Vehicle truck = {used + 1, 0.0, 0.0, local_garage[used], 0, {0}};
        bool added_any = false;
        
        while (true) {
            vector<Candidate> cands;
            for (int i = 1; i < (int)customers.size(); i++) {
                if (visited[i]) continue;
                Candidate cand = eval_move(truck, i);
                if (cand.customer != -1) cands.push_back(cand);
            }
            if (cands.empty()) break;
            
            Candidate best;
            if (version == 4) {
                sort(cands.begin(), cands.end(), better_v3);
                int k = min(5, (int)cands.size());
                uniform_int_distribution<> d(0, k - 1);
                best = cands[d(rng)];
            } else {
                auto comp = (version == 1) ? better_v1 : (version == 2) ? better_v2 : better_v3;
                sort(cands.begin(), cands.end(), comp);
                best = cands[0];
            }
            
            truck.current_load += customers[best.customer].demand;
            truck.clock_time = best.finish;
            truck.current_node = best.customer;
            truck.route.push_back(best.customer);
            visited[best.customer] = true;
            unvisited--;
            added_any = true;
        }
        if (!added_any) return Result{false};
        
        double ret = get_travel_time(get_distance(truck.current_node, 0), truck.clock_time);
        if (truck.clock_time + ret > DEPOT_DUE + EPS) return Result{false};
        
        truck.clock_time += ret;
        truck.route.push_back(0);
        res.total_time += truck.clock_time;
        res.fleet.push_back(truck);
        used++;
    }
    res.feasible = true;
    return res;
}

// --- CSV PARSER & AGGREGATION ENGINE ---
map<string, double> load_milp_baselines(const string& filename) {
    map<string, double> baselines;
    ifstream file(filename);
    if (!file) { cerr << "WARNING: Could not find " << filename << ". Ratios will be set to N/A.\n"; return baselines; }
    string line;
    getline(file, line); 
    while (getline(file, line)) {
        stringstream ss(line);
        string inst, intervals, n, ratio, time_str;
        getline(ss, inst, ','); getline(ss, intervals, ','); getline(ss, n, ','); getline(ss, ratio, ','); getline(ss, time_str, ','); 
        if (time_str != "N/A" && time_str != "FAIL" && time_str != "") baselines[inst] = stod(time_str);
        else baselines[inst] = -1.0; 
    }
    return baselines;
}

struct GroupStats {
    int inst_count = 0;
    double sum_dem_cap = 0;
    int count_v1 = 0; double sum_ratio_v1 = 0; double sum_veh_v1 = 0;
    int count_v2 = 0; double sum_ratio_v2 = 0; double sum_veh_v2 = 0;
    int count_v3 = 0; double sum_ratio_v3 = 0; double sum_veh_v3 = 0;
    int count_v4 = 0; double sum_ratio_v4 = 0; double sum_veh_v4 = 0;
};

void update_stats(GroupStats &stats, const Result &r, double best_sol, int version) {
    if (r.feasible) {
        double veh = r.fleet.size();
        
        if (best_sol > 0) {
            double ratio = r.total_time / best_sol;
            if (version == 1) { stats.count_v1++; stats.sum_ratio_v1 += ratio; }
            if (version == 2) { stats.count_v2++; stats.sum_ratio_v2 += ratio; }
            if (version == 3) { stats.count_v3++; stats.sum_ratio_v3 += ratio; }
            if (version == 4) { stats.count_v4++; stats.sum_ratio_v4 += ratio; }
        }
        
        if (version == 1) stats.sum_veh_v1 += veh;
        if (version == 2) stats.sum_veh_v2 += veh;
        if (version == 3) stats.sum_veh_v3 += veh;
        if (version == 4) stats.sum_veh_v4 += veh;
    }
}

void print_row(string col1, string col2, double dem_cap, const GroupStats& s) {
    cout << left << setw(8) << col1 << setw(6) << col2 
         << setw(10) << fixed << setprecision(2) << dem_cap;

    if (s.count_v1 > 0) cout << setw(10) << fixed << setprecision(3) << (s.sum_ratio_v1 / s.count_v1);
    else cout << setw(10) << "N/A";
    cout << setw(8) << fixed << setprecision(2) << (s.inst_count ? s.sum_veh_v1 / s.inst_count : 0.0);

    if (s.count_v2 > 0) cout << setw(10) << fixed << setprecision(3) << (s.sum_ratio_v2 / s.count_v2);
    else cout << setw(10) << "N/A";
    cout << setw(8) << fixed << setprecision(2) << (s.inst_count ? s.sum_veh_v2 / s.inst_count : 0.0);

    if (s.count_v3 > 0) cout << setw(10) << fixed << setprecision(3) << (s.sum_ratio_v3 / s.count_v3);
    else cout << setw(10) << "N/A";
    cout << setw(8) << fixed << setprecision(2) << (s.inst_count ? s.sum_veh_v3 / s.inst_count : 0.0);

    if (s.count_v4 > 0) cout << setw(10) << fixed << setprecision(3) << (s.sum_ratio_v4 / s.count_v4);
    else cout << setw(10) << "N/A";
    cout << setw(8) << fixed << setprecision(2) << (s.inst_count ? s.sum_veh_v4 / s.inst_count : 0.0);

    cout << "\n";
}

int main()
{
    map<string, double> milp_baselines = load_milp_baselines("milp_results_baseline.csv");
    ifstream file("TD_MALANDRAKI_32_Micro_Instances.txt");
    if (!file) { cout << "Could not open instances file.\n"; return 1; }

    mt19937 rng(2026);
    string layout;
    
    map<int, map<int, GroupStats>> table_data;
    GroupStats overall_stats;

    cout << "Processing 32 instances quietly...\n";

    while (load_next_instance(file, layout)) {
        int n = customers.size() - 1;
        int intervals = traffic_profile.size();
        
        double best_sol = milp_baselines.count(instance_name) ? milp_baselines[instance_name] : -1.0;

        double tot_dem = 0, tot_cap = 0;
        for (int i = 1; i <= n; i++) tot_dem += customers[i].demand;
        for (double c : garage_capacities) tot_cap += c;
        double current_ratio = (tot_cap > 0) ? (tot_dem / tot_cap) : 0;

        table_data[intervals][n].inst_count++;
        table_data[intervals][n].sum_dem_cap += current_ratio;
        overall_stats.inst_count++;
        overall_stats.sum_dem_cap += current_ratio;

        update_stats(table_data[intervals][n], solve_seqls(1, rng), best_sol, 1);
        update_stats(table_data[intervals][n], solve_seqls(2, rng), best_sol, 2);
        update_stats(table_data[intervals][n], solve_seqls(3, rng), best_sol, 3);
        
        Result best_r4;
        for (int i = 0; i < 20; i++) {
            Result r = solve_seqls(4, rng);
            if (r.feasible && r.total_time < best_r4.total_time) best_r4 = r;
        }
        update_stats(table_data[intervals][n], best_r4, best_sol, 4);
    }

    cout << "\n=======================================================================================================\n";
    cout << "TABLE V - RESULTS OF NEAREST-NEIGHBOR TDVRP HEURISTICS (SEQ-LS INTERNAL VERSIONS)\n";
    cout << "=======================================================================================================\n";
    cout << left << setw(8) << "Ints" << setw(6) << "N" << setw(10) << "Dem/Cap" 
         << setw(18) << "--- V1(Travel) ---" << setw(18) << "--- V2(Start) ----" 
         << setw(18) << "--- V3(Finish) ---" << setw(18) << "--- V4(Prob) -----" << "\n";
    cout << left << setw(8) << "per Lnk" << setw(6) << "" << setw(10) << "Ratio" 
         << setw(10) << "Heur/Opt" << setw(8) << "Veh" 
         << setw(10) << "Heur/Opt" << setw(8) << "Veh"
         << setw(10) << "Heur/Opt" << setw(8) << "Veh"
         << setw(10) << "Heur/Opt" << setw(8) << "Veh\n";
    cout << "-------------------------------------------------------------------------------------------------------\n";

    for (int interval : {2, 3}) {
        GroupStats avg_interval;
        int active_n_count = 0;
        
        for (int n : {4, 6, 8, 10}) {
            if (table_data[interval].count(n)) {
                GroupStats& s = table_data[interval][n];
                double dem_cap = s.sum_dem_cap / s.inst_count;
                print_row(to_string(interval), to_string(n), dem_cap, s);
                
                avg_interval.inst_count += s.inst_count;
                avg_interval.sum_dem_cap += dem_cap;
                avg_interval.count_v1 += s.count_v1; avg_interval.sum_ratio_v1 += s.sum_ratio_v1; avg_interval.sum_veh_v1 += s.sum_veh_v1;
                avg_interval.count_v2 += s.count_v2; avg_interval.sum_ratio_v2 += s.sum_ratio_v2; avg_interval.sum_veh_v2 += s.sum_veh_v2;
                avg_interval.count_v3 += s.count_v3; avg_interval.sum_ratio_v3 += s.sum_ratio_v3; avg_interval.sum_veh_v3 += s.sum_veh_v3;
                avg_interval.count_v4 += s.count_v4; avg_interval.sum_ratio_v4 += s.sum_ratio_v4; avg_interval.sum_veh_v4 += s.sum_veh_v4;
                active_n_count++;
            }
        }
        cout << "-------------------------------------------------------------------------------------------------------\n";
        print_row("Avg", "", avg_interval.sum_dem_cap / active_n_count, avg_interval);
        cout << "-------------------------------------------------------------------------------------------------------\n";
        
        overall_stats.count_v1 += avg_interval.count_v1; overall_stats.sum_ratio_v1 += avg_interval.sum_ratio_v1; overall_stats.sum_veh_v1 += avg_interval.sum_veh_v1;
        overall_stats.count_v2 += avg_interval.count_v2; overall_stats.sum_ratio_v2 += avg_interval.sum_ratio_v2; overall_stats.sum_veh_v2 += avg_interval.sum_veh_v2;
        overall_stats.count_v3 += avg_interval.count_v3; overall_stats.sum_ratio_v3 += avg_interval.sum_ratio_v3; overall_stats.sum_veh_v3 += avg_interval.sum_veh_v3;
        overall_stats.count_v4 += avg_interval.count_v4; overall_stats.sum_ratio_v4 += avg_interval.sum_ratio_v4; overall_stats.sum_veh_v4 += avg_interval.sum_veh_v4;
    }
    
    print_row("Overall", "", overall_stats.sum_dem_cap / 8.0, overall_stats);
    cout << "=======================================================================================================\n";

    return 0;
}