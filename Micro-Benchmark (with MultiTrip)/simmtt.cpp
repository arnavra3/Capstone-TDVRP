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

// --- DATA STRUCTURES ---
struct Interval { double start, end, speed; };
struct Customer { int id; double x, y, demand, ready, due, service; };
struct Vehicle { int id; double clock_time; double current_load; int current_node; vector<int> route; bool is_active; double max_capacity; };
struct Candidate { int customer = -1; int vehicle_idx = -1; double travel = 1e18, arrive = 1e18, start = 1e18, finish = 1e18; };
struct Result { bool feasible = false; double total_time = 1e18; vector<Vehicle> fleet; };

// --- GLOBAL CONTEXT ---
vector<Interval> traffic_profile;
vector<Customer> customers;
vector<double> garage_capacities;
string instance_name;
double DEPOT_DUE;
int TOTAL_VEHICLES;
static const double EPS = 1e-9;
const double RELOAD_TIME = 30.0; // Multi-Trip Penalty Time

// --- UTILITY FUNCTIONS ---
double get_distance(int n1, int n2) { return hypot(customers[n1].x - customers[n2].x, customers[n1].y - customers[n2].y); }

double get_travel_time(double dist, double start_time) {
    double speed = traffic_profile.back().speed; 
    for (const auto &iv : traffic_profile) {
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
    for (int i = 0; i < TOTAL_VEHICLES; i++) {
        double cap; file >> cap; garage_capacities.push_back(cap);
    }

    int m;
    file >> dummy >> m;
    file >> dummy >> dummy >> dummy;
    for (int i = 0; i < m; i++) {
        Interval iv; file >> iv.start >> iv.end >> iv.speed; traffic_profile.push_back(iv);
    }
    
    file >> dummy; for (int i = 0; i < 7; i++) file >> dummy;
    for (int i = 0; i <= num_cust; i++) {
        Customer c; file >> c.id >> c.x >> c.y >> c.demand >> c.ready >> c.due >> c.service;
        customers.push_back(c);
    }
    DEPOT_DUE = customers[0].due;
    return true;
}

bool better_v3(const Candidate &a, const Candidate &b) {
    if (a.customer == -1) return false;
    if (b.customer == -1) return true;
    if (abs(a.finish - b.finish) > EPS) return a.finish < b.finish;
    return a.customer < b.customer;
}

Candidate eval_hetero_move(const Vehicle &v, int cust_id, bool is_new, double custom_cap = 0) {
    Candidate c;
    double cap = is_new ? custom_cap : v.max_capacity;
    if (v.current_load + customers[cust_id].demand > cap + EPS) return c;

    double depart = is_new ? 0.0 : v.clock_time;
    int from = is_new ? 0 : v.current_node;

    c.customer = cust_id;
    c.travel = get_travel_time(get_distance(from, cust_id), depart);
    c.arrive = depart + c.travel;
    c.start = max(c.arrive, customers[cust_id].ready);
    c.finish = c.start + customers[cust_id].service;

    if (c.finish > customers[cust_id].due + EPS) return Candidate();
    return c;
}

// --- CORE SIMULTANEOUS MULTI-TRIP SOLVER ---
Result solve_sim(bool is_probabilistic, mt19937 &rng) {
    Result res;
    res.total_time = 0.0;
    vector<bool> visited(customers.size(), false);
    visited[0] = true;
    int unvisited = (int)customers.size() - 1;
    vector<Vehicle> fleet;
    vector<double> garage = garage_capacities;

    while (unvisited > 0) {
        vector<Candidate> valid_moves;

        // 1. Check Active Fleet
        for (int v = 0; v < (int)fleet.size(); v++) {
            bool made_customer_move = false;
            for (int i = 1; i < (int)customers.size(); i++) {
                if (visited[i]) continue;
                Candidate cand = eval_hetero_move(fleet[v], i, false);
                if (cand.customer != -1) {
                    cand.vehicle_idx = v;
                    valid_moves.push_back(cand);
                    made_customer_move = true;
                }
            }
            
            // MULTI-TRIP DYNAMIC RETURN: If stuck due to capacity, evaluate returning to depot
            if (!made_customer_move && fleet[v].current_node != 0) {
                double travel = get_travel_time(get_distance(fleet[v].current_node, 0), fleet[v].clock_time);
                if (fleet[v].clock_time + travel + RELOAD_TIME <= DEPOT_DUE) {
                    Candidate cand;
                    cand.customer = 0; // 0 signals depot return
                    cand.vehicle_idx = v;
                    cand.travel = travel;
                    cand.arrive = fleet[v].clock_time + travel;
                    cand.start = cand.arrive;
                    cand.finish = cand.start + RELOAD_TIME;
                    valid_moves.push_back(cand);
                }
            }
        }

        // 2. Check Garage (Best-Fit Logic)
        if (!garage.empty()) {
            for (int i = 1; i < (int)customers.size(); i++) {
                if (visited[i]) continue;
                int best_g_idx = -1;
                for (int g = 0; g < (int)garage.size(); g++) {
                    if (garage[g] >= customers[i].demand - EPS) {
                        if (best_g_idx == -1 || garage[g] < garage[best_g_idx])
                            best_g_idx = g;
                    }
                }
                if (best_g_idx != -1) {
                    Vehicle tmp = {0, 0.0, 0.0, 0, {0}, false, garage[best_g_idx]};
                    Candidate cand = eval_hetero_move(tmp, i, true, garage[best_g_idx]);
                    if (cand.customer != -1) {
                        cand.vehicle_idx = -2;
                        valid_moves.push_back(cand);
                    }
                }
            }
        }

        if (valid_moves.empty()) return Result{false};
        
        sort(valid_moves.begin(), valid_moves.end(), better_v3);

        Candidate best;
        if (is_probabilistic) {
            int top_k = min(5, (int)valid_moves.size());
            uniform_int_distribution<> d(0, top_k - 1);
            best = valid_moves[d(rng)];
        } else {
            best = valid_moves[0];
        }

        if (best.vehicle_idx == -2) {
            // Woke up a new truck from garage
            int g_idx = -1;
            for (int i = 0; i < (int)garage.size(); i++) {
                if (garage[i] >= customers[best.customer].demand - EPS) {
                    if (g_idx == -1 || garage[i] < garage[g_idx]) g_idx = i;
                }
            }
            Vehicle nv = {(int)fleet.size() + 1, best.finish, customers[best.customer].demand, best.customer, {0, best.customer}, true, garage[g_idx]};
            fleet.push_back(nv);
            garage.erase(garage.begin() + g_idx);
            visited[best.customer] = true;
            unvisited--;
        } 
        else if (best.customer == 0) {
            // MULTI-TRIP: Truck dynamically returned to reload
            Vehicle &v = fleet[best.vehicle_idx];
            v.clock_time = best.finish;
            v.current_node = 0;
            v.current_load = 0.0;
            v.route.push_back(0);
            // Notice we do NOT decrement 'unvisited' here
        } 
        else {
            // Normal customer visit
            Vehicle &v = fleet[best.vehicle_idx];
            v.current_load += customers[best.customer].demand;
            v.clock_time = best.finish;
            v.current_node = best.customer;
            v.route.push_back(best.customer);
            visited[best.customer] = true;
            unvisited--;
        }
    }

    // Return all vehicles to depot at end of day
    for (auto &v : fleet) {
        if (v.current_node != 0) {
            double ret = get_travel_time(get_distance(v.current_node, 0), v.clock_time);
            if (v.clock_time + ret > DEPOT_DUE + EPS) return Result{false};
            v.clock_time += ret;
            v.route.push_back(0);
        }
        res.total_time += v.clock_time;
    }
    
    res.fleet = fleet;
    res.feasible = true;
    return res;
}

// --- CSV PARSER & AGGREGATION ENGINE ---
map<string, double> load_milp_baselines(const string& filename) {
    map<string, double> baselines;
    ifstream file(filename);
    if (!file) return baselines;
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
    int count_sim = 0; double sum_ratio_sim = 0; double sum_veh_sim = 0;
    int count_prob = 0; double sum_ratio_prob = 0; double sum_veh_prob = 0;
};

void update_stats(GroupStats &stats, const Result &r, double best_sol, bool is_prob) {
    if (r.feasible && best_sol > 0) {
        double veh = r.fleet.size();
        double ratio = r.total_time / best_sol;
        
        if (!is_prob) { 
            stats.count_sim++; 
            stats.sum_ratio_sim += ratio; 
            stats.sum_veh_sim += veh; 
        } else { 
            stats.count_prob++; 
            stats.sum_ratio_prob += ratio; 
            stats.sum_veh_prob += veh; 
        }
    }
}

void print_row(string col1, string col2, double dem_cap, const GroupStats& s) {
    cout << left << setw(8) << col1 << setw(6) << col2 
         << setw(10) << fixed << setprecision(2) << dem_cap;

    if (s.count_sim > 0) {
        cout << setw(10) << fixed << setprecision(3) << (s.sum_ratio_sim / s.count_sim);
        cout << setw(12) << fixed << setprecision(2) << (s.sum_veh_sim / s.count_sim);
    } else {
        cout << setw(10) << "N/A" << setw(12) << "N/A";
    }

    if (s.count_prob > 0) {
        cout << setw(10) << fixed << setprecision(3) << (s.sum_ratio_prob / s.count_prob);
        cout << setw(8) << fixed << setprecision(2) << (s.sum_veh_prob / s.count_prob);
    } else {
        cout << setw(10) << "N/A" << setw(8) << "N/A";
    }

    cout << "\n";
}

int main() {
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

        update_stats(table_data[intervals][n], solve_sim(false, rng), best_sol, false);
        
        Result best_prob;
        for (int i = 0; i < 20; i++) {
            Result r = solve_sim(true, rng);
            if (r.feasible && r.total_time < best_prob.total_time) best_prob = r;
        }
        update_stats(table_data[intervals][n], best_prob, best_sol, true);
    }

    cout << "\n=================================================================================\n";
    cout << "TABLE V - RESULTS OF NEAREST-NEIGHBOR TDVRP HEURISTICS (SIMULTANEOUS MULTI-TRIP)\n";
    cout << "=================================================================================\n";
    cout << left << setw(8) << "Ints" << setw(6) << "N" << setw(10) << "Dem/Cap" 
         << setw(22) << "------ SIM ------" << setw(18) << "--- SIM+PROB ---" << "\n";
    cout << left << setw(8) << "per Lnk" << setw(6) << "" << setw(10) << "Ratio" 
         << setw(10) << "Heur/Opt" << setw(12) << "Vehicles" 
         << setw(10) << "Heur/Opt" << setw(8) << "Vehicles\n";
    cout << "---------------------------------------------------------------------------------\n";

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
                avg_interval.count_sim += s.count_sim; avg_interval.sum_ratio_sim += s.sum_ratio_sim; avg_interval.sum_veh_sim += s.sum_veh_sim;
                avg_interval.count_prob += s.count_prob; avg_interval.sum_ratio_prob += s.sum_ratio_prob; avg_interval.sum_veh_prob += s.sum_veh_prob;
                active_n_count++;
            }
        }
        cout << "---------------------------------------------------------------------------------\n";
        print_row("Avg", "", avg_interval.sum_dem_cap / active_n_count, avg_interval);
        cout << "---------------------------------------------------------------------------------\n";
        
        overall_stats.count_sim += avg_interval.count_sim; overall_stats.sum_ratio_sim += avg_interval.sum_ratio_sim; overall_stats.sum_veh_sim += avg_interval.sum_veh_sim;
        overall_stats.count_prob += avg_interval.count_prob; overall_stats.sum_ratio_prob += avg_interval.sum_ratio_prob; overall_stats.sum_veh_prob += avg_interval.sum_veh_prob;
    }
    
    print_row("Overall", "", overall_stats.sum_dem_cap / 8.0, overall_stats);
    cout << "=================================================================================\n";

    return 0;
}