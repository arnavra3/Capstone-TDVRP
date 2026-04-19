import random
import math

NUM_DATASETS = 11
LAYOUTS = ["C", "R", "RC"]
SIZES = [10, 11, 12] # The "Sweet Spot" for MILP vs Heuristic comparison

# Depot due dates adjusted for 10-12 customers
DEPOT_DUE_BY_SIZE = {10: 700, 11: 750, 12: 800}
# Tighter fleet sizes to make the packing problem harder
TARGET_VEHICLES = {10: 2, 11: 4, 12: 4} 

def clamp(v, lo=0, hi=100):
    return max(lo, min(hi, v))

def cluster_point(rng, cx, cy, spread):
    return clamp(int(rng.gauss(cx, spread))), clamp(int(rng.gauss(cy, spread)))

def gen_customer(rng, layout, idx, n):
    depot_due = DEPOT_DUE_BY_SIZE[n]
    if layout == "C":
        centers = [(15, 15), (85, 15), (50, 85)]
        cx, cy = centers[(idx - 1) % 3]
        x, y = cluster_point(rng, cx, cy, 8)
        ready_band = [(0, 150), (100, 250), (200, 400)][(idx - 1) % 3]
        ready = rng.randint(*ready_band)
        window = rng.randint(60, 110) 
    elif layout == "R":
        x, y = rng.randint(0, 100), rng.randint(0, 100)
        dist = math.hypot(x - 50, y - 50)
        base_time = int((dist / 70.0) * (depot_due * 0.40))
        ready = base_time + rng.randint(0, 100)
        window = rng.randint(60, 110) 
    else:  # RC
        if idx % 2 == 0:
            x, y = cluster_point(rng, 20, 80, 10)
        else:
            x, y = rng.randint(0, 100), rng.randint(0, 100)
        dist = math.hypot(x - 50, y - 50)
        base_time = int((dist / 70.0) * (depot_due * 0.40))
        ready = base_time + rng.randint(0, 100)
        window = rng.randint(60, 110)

    demand = rng.randint(20, 45) # Heavy demand
    service = rng.randint(10, 20)
    due = ready + window
    return x, y, demand, ready, due, service

def generate_all_datasets():
    filename = "TD_MICRO_11_INSTANCES.txt"
    rng = random.Random(202603)

    with open(filename, "w") as f:
        f.write(f"TOTAL_INSTANCES {NUM_DATASETS}\n\n")

        for file_id in range(1, NUM_DATASETS + 1):
            layout = rng.choice(LAYOUTS)
            num_cust = rng.choice(SIZES)
            depot_due = DEPOT_DUE_BY_SIZE[num_cust]
            total_k = TARGET_VEHICLES[num_cust]

            # 5 Complex Intervals
            intervals = [
                (0, 200, 1.2),
                (200, 400, 0.35), # The Trap
                (400, 600, 0.7),  # Slow recovery
                (600, 850, 1.4),  # Clear road
                (850, 2000, 1.0)
            ]

            customers = []
            total_demand = 0
            max_cust_demand = 0
            for i in range(1, num_cust + 1):
                res = gen_customer(rng, layout, i, num_cust)
                customers.append(res)
                total_demand += res[2]
                max_cust_demand = max(max_cust_demand, res[2])

            avg_required_cap = (total_demand / total_k) * 1.2
            truck_capacities = []
            for _ in range(total_k):
                multiplier = rng.triangular(0.8, 1.0, 1.6) 
                cap = int(avg_required_cap * multiplier)
                cap = max(cap, max_cust_demand + 10)
                truck_capacities.append(cap)
            
            truck_capacities.sort(reverse=True) 

            name = f"TD_HARD_{layout}_{num_cust}_{file_id:03d}"
            f.write(f"INSTANCE {name}\n")
            f.write(f"LAYOUT {layout}\n")
            f.write(f"CUSTOMERS {num_cust}\n")
            f.write(f"TOTAL_VEHICLES {total_k}\n")
            
            f.write(f"VEHICLE_CAPACITIES\n")
            f.write(" ".join(map(str, truck_capacities)) + "\n")

            f.write(f"M_INTERVALS {len(intervals)}\n")
            f.write("START    END      SPEED_MULTIPLIER\n")
            for start, end, speed in intervals:
                f.write(f"{start:<8} {end:<8} {speed}\n")

            f.write("CUSTOMER_DATA\n")
            f.write("CUST_NO  XCOORD  YCOORD  DEMAND  READY_TIME  DUE_DATE  SERVICE_TIME\n")
            f.write(f"0        50      50      0       0           {depot_due}       0\n")

            for i, (x, y, demand, ready, due, service) in enumerate(customers, 1):
                due = min(due, depot_due - service - 20)
                if ready >= due: ready = max(0, due - 30)
                f.write(f"{i:<8} {x:<7} {y:<7} {demand:<7} {ready:<11} {due:<9} {service}\n")
            f.write("\n")

    print(f"Generated {NUM_DATASETS} balanced micro-hard instances in {filename}")

if __name__ == "__main__":
    generate_all_datasets()