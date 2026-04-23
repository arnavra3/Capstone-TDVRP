import math
from ortools.linear_solver import pywraplp

def calculate_distance(x1, y1, x2, y2):
    return math.hypot(x1 - x2, y1 - y2)

def solve_instance(inst):
    print(f"\n{'='*50}")
    print(f"SOLVING INSTANCE: {inst['name']} ({inst['num_cust']} Cust, {inst['total_veh']} Veh)")
    
    solver = pywraplp.Solver.CreateSolver('SCIP')
    if not solver:
        return
    
    # Time limit: 5 minutes (300,000 milliseconds) per instance
    solver.SetTimeLimit(300000)

    N = inst['num_cust']
    K = inst['total_veh']
    
    # Nodes: 0 (Start Depot), 1 to N (Customers), N+1 to N+K (Return Depots for each vehicle)
    nodes = list(range(0, N + K + 1))
    customers = list(range(1, N + 1))
    return_depots = list(range(N + 1, N + K + 1))
    intervals = list(range(len(inst['intervals'])))

    # Extract Data
    c_data = inst['cust_data']
    demands = {i: c_data[i]['demand'] for i in range(N + 1)}
    services = {i: c_data[i]['service'] for i in range(N + 1)}
    readys = {i: c_data[i]['ready'] for i in range(N + 1)}
    dues = {i: c_data[i]['due'] for i in range(N + 1)}
    
    # Return depots have 0 demand, 0 service, and same time window as Start Depot
    for i in return_depots:
        demands[i] = 0
        services[i] = 0
        readys[i] = readys[0]
        dues[i] = dues[0]
        
    B1 = 5000 # Big-M for time
    B = 500   # Big-M for capacity
    
    # Calculate Travel Times (Distance / Speed)
    travel_time = {}
    for i in nodes:
        for j in nodes:
            if i == j: continue
            
            # Map i and j to their actual coordinates (return depots are physically at node 0)
            real_i = 0 if i > N else i
            real_j = 0 if j > N else j
            
            dist = calculate_distance(c_data[real_i]['x'], c_data[real_i]['y'], 
                                      c_data[real_j]['x'], c_data[real_j]['y'])
            
            for m_idx, (start, end, speed) in enumerate(inst['intervals']):
                travel_time[(i, j, m_idx)] = dist / speed

    # --- VARIABLES ---
    x = {}
    for i in nodes:
        for j in nodes:
            if i != j:
                for m in intervals:
                    x[(i, j, m)] = solver.IntVar(0, 1, f'x_{i}_{j}_{m}')
                    
    t = {i: solver.NumVar(0, solver.infinity(), f't_{i}') for i in nodes}
    w = {i: solver.NumVar(0, solver.infinity(), f'w_{i}') for i in nodes}

    # --- CONSTRAINTS ---
    # 1. Each customer visited exactly once
    for j in customers:
        solver.Add(sum(x[(i, j, m)] for i in nodes if i != j for m in intervals) == 1)

    # 2. Leave starting depot exactly K times
    solver.Add(sum(x[(0, j, m)] for j in customers + return_depots for m in intervals) == K)

    # 3. Flow conservation
    for p in customers:
        solver.Add(
            sum(x[(i, p, m)] for i in nodes if i != p for m in intervals) == 
            sum(x[(p, j, m)] for j in nodes if j != p for m in intervals)
        )

    # 4. Each return depot receives exactly 1 connection (forces K separate routes)
    for d in return_depots:
        solver.Add(sum(x[(i, d, m)] for i in nodes if i != d for m in intervals) == 1)

    # 5. Start time
    solver.Add(t[0] == 0)

    # 6. Time Tracking & Intervals
    for i in nodes:
        for j in nodes:
            if i != j:
                for m_idx, (m_start, m_end, _) in enumerate(inst['intervals']):
                    # Arrival time calculation
                    solver.Add(t[j] - t[i] - B1 * x[(i, j, m_idx)] >= travel_time[(i, j, m_idx)] + services[j] - B1)
                    # Must leave within the selected interval
                    solver.Add(t[i] - m_start * x[(i, j, m_idx)] >= 0)
                    solver.Add(t[i] + B1 * x[(i, j, m_idx)] <= m_end + B1)

    # 7. Time Windows
    for i in nodes:
        solver.Add(t[i] >= readys[i] + services[i])
        solver.Add(t[i] <= dues[i] + services[i])

    # 8. Capacity Tracking
    solver.Add(w[0] == 0)
    for i in nodes:
        for j in nodes:
            if i != j:
                for m in intervals:
                    solver.Add(w[j] - w[i] - B * x[(i, j, m)] >= demands[j] - B)
                    
    # Heterogeneous Fleet Capacities (Map specific capacities to specific return depots)
    for idx, d in enumerate(return_depots):
        solver.Add(w[d] <= inst['capacities'][idx])

    # --- OBJECTIVE: Minimize sum of return times ---
    solver.Minimize(sum(t[d] for d in return_depots))

    print("Solving... (Max 5 minutes)")
    status = solver.Solve()

    if status == pywraplp.Solver.OPTIMAL:
        print(f"SUCCESS! (100% OPTIMAL) Route Total Time: {solver.Objective().Value():.2f}")
        
    elif status == pywraplp.Solver.FEASIBLE:
        print(f"SUCCESS! (TIME LIMIT REACHED - BEST FOUND) Route Total Time: {solver.Objective().Value():.2f}")
        
    elif status == pywraplp.Solver.INFEASIBLE:
        print("FAILED: NO SOLUTION EXISTS (The problem is mathematically impossible with these constraints).")
        return # Exit the function since there's no path to print

    elif status == pywraplp.Solver.NOT_SOLVED:
        print("FAILED: TIME LIMIT EXCEEDED (Solver timed out before finding any valid path).")
        return # Exit the function
        
    else:
        print(f"FAILED: Unknown solver status ({status}).")
        return

    # This block should only run if a solution was actually found
    if status == pywraplp.Solver.OPTIMAL or status == pywraplp.Solver.FEASIBLE:
        print("--- Route Sequence ---")
        # ... (Your path extraction logic follows here, also indented)
        
        # --- PATH EXTRACTION LOGIC ---
        outgoing = {}
        starts = []
        for i in nodes:
            for j in nodes:
                if i != j:
                    for m in intervals:
                        if x[(i, j, m)].solution_value() > 0.5:
                            if i == 0:
                                starts.append(j)
                            else:
                                outgoing[i] = j
                                
        # Trace the path for each vehicle
        for start_node in starts:
            route = [0]
            curr = start_node
            
            # Follow the chain until we hit a return depot
            while curr not in return_depots:
                route.append(curr)
                curr = outgoing[curr]
                
            veh_id = curr - N # Convert return depot node ID back to vehicle ID (1, 2, 3...)
            veh_cap = inst['capacities'][veh_id - 1]
            route.append(0) # Representing arriving back at the depot
            
            route_str = " -> ".join(map(str, route))
            arr_time = t[curr].solution_value()
            
            # If a vehicle isn't used, it goes 0 -> 0. We print it to match C++.
            print(f"  Vehicle {veh_id} (Cap {veh_cap}): {route_str} [Arrived back at {arr_time:.2f} mins]")
            
    else:
        print("NO SOLUTION FOUND or TIME LIMIT REACHED.")


def parse_and_solve(filename):
    with open(filename, 'r') as f:
        lines = [line.strip() for line in f if line.strip()]
    
    idx = 1 # Skip TOTAL_INSTANCES line
    
    while idx < len(lines):
        inst = {'intervals': [], 'cust_data': {}}
        
        inst['name'] = lines[idx].split()[1]; idx += 1
        inst['layout'] = lines[idx].split()[1]; idx += 1
        inst['num_cust'] = int(lines[idx].split()[1]); idx += 1
        inst['total_veh'] = int(lines[idx].split()[1]); idx += 1
        
        idx += 1 # Skip VEHICLE_CAPACITIES header
        inst['capacities'] = list(map(int, lines[idx].split())); idx += 1
        
        m_intervals = int(lines[idx].split()[1]); idx += 1
        idx += 1 # Skip START END SPEED header
        
        for _ in range(m_intervals):
            parts = lines[idx].split()
            inst['intervals'].append((float(parts[0]), float(parts[1]), float(parts[2])))
            idx += 1
            
        idx += 2 # Skip CUSTOMER_DATA and its header
        
        for _ in range(inst['num_cust'] + 1):
            parts = lines[idx].split()
            c_id = int(parts[0])
            inst['cust_data'][c_id] = {
                'x': float(parts[1]), 'y': float(parts[2]),
                'demand': int(parts[3]), 'ready': int(parts[4]),
                'due': int(parts[5]), 'service': int(parts[6])
            }
            idx += 1
            
        solve_instance(inst)

if __name__ == '__main__':
    parse_and_solve("TD_MICRO_11_Instances.txt")