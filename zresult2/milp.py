import math
import csv
from ortools.linear_solver import pywraplp

def calculate_distance(x1, y1, x2, y2):
    return math.hypot(x1 - x2, y1 - y2)

def solve_instance(inst):
    print(f"\n{'='*50}")
    print(f"SOLVING INSTANCE: {inst['name']} ({inst['num_cust']} Cust, {inst['total_veh']} Veh)")
    
    solver = pywraplp.Solver.CreateSolver('SCIP')
    if not solver:
        return None
    
    # Time limit: 10 minutes (600000 milliseconds) per instance
    solver.SetTimeLimit(600000)

    N = inst['num_cust']
    K = inst['total_veh']
    intervals = list(range(len(inst['intervals'])))
    
    nodes = list(range(0, N + K + 1))
    customers = list(range(1, N + 1))
    return_depots = list(range(N + 1, N + K + 1))

    c_data = inst['cust_data']
    demands = {i: c_data[i]['demand'] for i in range(N + 1)}
    services = {i: c_data[i]['service'] for i in range(N + 1)}
    readys = {i: c_data[i]['ready'] for i in range(N + 1)}
    dues = {i: c_data[i]['due'] for i in range(N + 1)}
    
    for i in return_depots:
        demands[i] = 0
        services[i] = 0
        readys[i] = readys[0]
        dues[i] = dues[0]
        
    B1 = 5000 
    B = 500  
    
    # Metric Calculations for Table V
    total_demand = sum(demands[i] for i in customers)
    total_capacity = sum(inst['capacities'])
    dem_cap_ratio = total_demand / total_capacity if total_capacity > 0 else 0
    
    travel_time = {}
    for i in nodes:
        for j in nodes:
            if i == j: continue
            real_i = 0 if i > N else i
            real_j = 0 if j > N else j
            dist = calculate_distance(c_data[real_i]['x'], c_data[real_i]['y'], 
                                      c_data[real_j]['x'], c_data[real_j]['y'])
            for m_idx, (start, end, speed) in enumerate(inst['intervals']):
                travel_time[(i, j, m_idx)] = dist / speed

    # --- SAFE PRUNING ADDED BACK ---
    x = {}
    for i in nodes:
        for j in nodes:
            if i != j:
                for m_idx, (m_start, m_end, _) in enumerate(inst['intervals']):
                    # Pruning physically impossible time windows
                    if i in customers and readys[i] + services[i] > m_end:
                        continue 
                    if i in customers and j in customers:
                        if readys[i] + services[i] + travel_time[(i, j, m_idx)] > dues[j]:
                            continue 
                            
                    x[(i, j, m_idx)] = solver.IntVar(0, 1, f'x_{i}_{j}_{m_idx}')
                    
    t = {i: solver.NumVar(0, solver.infinity(), f't_{i}') for i in nodes}
    w = {i: solver.NumVar(0, solver.infinity(), f'w_{i}') for i in nodes}

    # Constrain only the generated (valid) variables
    for j in customers:
        valid_incoming = [x[(i, j, m)] for i in nodes for m in intervals if (i, j, m) in x]
        if valid_incoming:
            solver.Add(sum(valid_incoming) == 1)

    valid_depot_out = [x[(0, j, m)] for j in customers + return_depots for m in intervals if (0, j, m) in x]
    solver.Add(sum(valid_depot_out) == K)

    for p in customers:
        incoming = [x[(i, p, m)] for i in nodes for m in intervals if (i, p, m) in x]
        outgoing = [x[(p, j, m)] for j in nodes for m in intervals if (p, j, m) in x]
        solver.Add(sum(incoming) == sum(outgoing))

    for d in return_depots:
        valid_returns = [x[(i, d, m)] for i in nodes for m in intervals if (i, d, m) in x]
        if valid_returns:
            solver.Add(sum(valid_returns) == 1)

    solver.Add(t[0] == 0)

    for i in nodes:
        for j in nodes:
            if i != j:
                for m_idx, (m_start, m_end, _) in enumerate(inst['intervals']):
                    if (i, j, m_idx) in x:
                        solver.Add(t[j] - t[i] - B1 * x[(i, j, m_idx)] >= travel_time[(i, j, m_idx)] + services[j] - B1)
                        solver.Add(t[i] - m_start * x[(i, j, m_idx)] >= 0)
                        solver.Add(t[i] + B1 * x[(i, j, m_idx)] <= m_end + B1)

    for i in nodes:
        solver.Add(t[i] >= readys[i] + services[i])
        solver.Add(t[i] <= dues[i] + services[i])

    solver.Add(w[0] == 0)
    for i in nodes:
        for j in nodes:
            if i != j:
                for m in intervals:
                    if (i, j, m) in x:
                        solver.Add(w[j] - w[i] - B * x[(i, j, m)] >= demands[j] - B)
                    
    for idx, d in enumerate(return_depots):
        solver.Add(w[d] <= inst['capacities'][idx])

    solver.Minimize(sum(t[d] for d in return_depots))

    print("Solving... (Max 10 minutes)")
    status = solver.Solve()
    
    result_data = {
        'name': inst['name'],
        'intervals': len(intervals),
        'n': N,
        'ratio': dem_cap_ratio,
        'time': None,
        'veh': None,
        'status': 'FAIL'
    }

    if status == pywraplp.Solver.OPTIMAL or status == pywraplp.Solver.FEASIBLE:
        obj_val = solver.Objective().Value()
        result_data['time'] = obj_val
        result_data['status'] = 'OPT' if status == pywraplp.Solver.OPTIMAL else 'FEAS'
        
        print(f"SUCCESS! Route Total Time: {obj_val:.2f}")
        
        outgoing = {}
        starts = []
        for i in nodes:
            for j in nodes:
                if i != j:
                    for m in intervals:
                        if (i, j, m) in x and x[(i, j, m)].solution_value() > 0.5:
                            if i == 0:
                                starts.append(j)
                            else:
                                outgoing[i] = j
                                
        used_vehicles = 0
        for start_node in starts:
            route = [0]
            curr = start_node
            
            if curr <= N: 
                used_vehicles += 1
                
            while curr not in return_depots:
                route.append(curr)
                if curr in outgoing:
                    curr = outgoing[curr]
                else:
                    break
                
            veh_id = curr - N 
            veh_cap = inst['capacities'][veh_id - 1]
            route.append(0) 
            
            route_str = " -> ".join(map(str, route))
            arr_time = t[curr].solution_value()
            print(f"  Vehicle {veh_id} (Cap {veh_cap}): {route_str} [Arrived back at {arr_time:.2f} mins]")
            
        result_data['veh'] = used_vehicles
        
    elif status == pywraplp.Solver.INFEASIBLE:
        print("FAILED: INFEASIBLE.")
    else:
        print("FAILED: TIME LIMIT / NOT SOLVED.")

    return result_data

def parse_and_solve(filename):
    with open(filename, 'r') as f:
        lines = [line.strip() for line in f if line.strip()]
    
    idx = 1 
    results = []
    
    while idx < len(lines):
        inst = {'intervals': [], 'cust_data': {}}
        
        inst['name'] = lines[idx].split()[1]; idx += 1
        inst['layout'] = lines[idx].split()[1]; idx += 1
        inst['num_cust'] = int(lines[idx].split()[1]); idx += 1
        inst['total_veh'] = int(lines[idx].split()[1]); idx += 1
        
        idx += 1 
        inst['capacities'] = list(map(int, lines[idx].split())); idx += 1
        
        m_intervals = int(lines[idx].split()[1]); idx += 1
        idx += 1 
        
        for _ in range(m_intervals):
            parts = lines[idx].split()
            inst['intervals'].append((float(parts[0]), float(parts[1]), float(parts[2])))
            idx += 1
            
        idx += 2 
        
        for _ in range(inst['num_cust'] + 1):
            parts = lines[idx].split()
            c_id = int(parts[0])
            inst['cust_data'][c_id] = {
                'x': float(parts[1]), 'y': float(parts[2]),
                'demand': int(parts[3]), 'ready': int(parts[4]),
                'due': int(parts[5]), 'service': int(parts[6])
            }
            idx += 1
            
        res = solve_instance(inst)
        if res:
            results.append(res)

    # --- PRINT TERMINAL TABLE ---
    print("\n" + "="*80)
    print("BENCHMARK SUMMARY (TABLE V FORMAT - MILP BASELINE)")
    print("="*80)
    print(f"{'Int':<5} | {'n':<4} | {'Dem/Cap':<9} | {'MILP Best Sol (Time)':<22} | {'Veh Used':<10} | {'Status'}")
    print("-" * 80)
    
    for r in results:
        time_str = f"{r['time']:.2f}" if r['time'] is not None else "N/A"
        veh_str = str(r['veh']) if r['veh'] is not None else "N/A"
        print(f"{r['intervals']:<5} | {r['n']:<4} | {r['ratio']:<9.2f} | {time_str:<22} | {veh_str:<10} | {r['status']}")

    # --- EXPORT TO CSV FOR MERGING TOMORROW ---
    csv_filename = "zresult2/milp_results_baseline.csv"
    with open(csv_filename, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(['Instance', 'Intervals', 'N', 'Dem_Cap_Ratio', 'MILP_Time', 'MILP_Veh', 'Status'])
        for r in results:
            time_val = f"{r['time']:.2f}" if r['time'] is not None else "N/A"
            veh_val = str(r['veh']) if r['veh'] is not None else "N/A"
            ratio_val = f"{r['ratio']:.4f}"
            writer.writerow([r['name'], r['intervals'], r['n'], ratio_val, time_val, veh_val, r['status']])
            
    print(f"\n[+] Successfully saved benchmark data to {csv_filename}")

if __name__ == '__main__':
    parse_and_solve("zresult2/TD_MALANDRAKI_32_Instances.txt")