import random
import math

NUM_DATASETS = 500
LAYOUTS = ["C", "R", "RC"]
SIZES = [25, 50, 100]

DEPOT_DUE_BY_SIZE = {25: 900, 50: 1100, 100: 1300}
TARGET_VEHICLES = {25: 8, 50: 15, 100: 28}

def clamp(v, lo=0, hi=100):
    return max(lo, min(hi, v))

def cluster_point(rng, cx, cy, spread):
    return clamp(int(rng.gauss(cx, spread))), clamp(int(rng.gauss(cy, spread)))

def gen_customer(rng, layout, idx, n):
    depot_due = DEPOT_DUE_BY_SIZE[n]
    if layout == "C":
        centers = [(18, 18), (82, 20), (50, 82)]
        cx, cy = centers[(idx - 1) % 3]
        x, y = cluster_point(rng, cx, cy, 7)
        ready_band = [(0, 300), (200, 500), (350, 650)][(idx - 1) % 3]
        ready = rng.randint(*ready_band)
        window = rng.randint(60, 110) 
    elif layout == "R":
        x, y = rng.randint(0, 100), rng.randint(0, 100)
        dist = math.hypot(x - 50, y - 50)
        base_time = int((dist / 70.0) * (depot_due * 0.45))
        ready = base_time + rng.randint(0, 150)
        window = rng.randint(60, 110) 
    else:  # RC
        if idx % 2 == 0:
            x, y = cluster_point(rng, 22, 78, 9)
        else:
            x, y = rng.randint(0, 100), rng.randint(0, 100)
        dist = math.hypot(x - 50, y - 50)
        base_time = int((dist / 70.0) * (depot_due * 0.45))
        ready = base_time + rng.randint(0, 150)
        window = rng.randint(60, 110)

    demand = rng.randint(10, 25)
    service = rng.randint(10, 20)
    due = ready + window
    return x, y, demand, ready, due, service

def generate_all_datasets():
    filename = "TD_HETERO_UNIQUE_500_Instances.txt"
    rng = random.Random(202603)

    with open(filename, "w") as f:
        f.write(f"TOTAL_INSTANCES {NUM_DATASETS}\n\n")

        for file_id in range(1, NUM_DATASETS + 1):
            layout = rng.choice(LAYOUTS)
            num_cust = rng.choice(SIZES)
            depot_due = DEPOT_DUE_BY_SIZE[num_cust]
            total_k = TARGET_VEHICLES[num_cust]

            # --- DYNAMIC TRAFFIC MULTIPLIERS ---
            m1 = round(rng.uniform(1.0, 1.3), 2)  # Morning fast
            m2 = round(rng.uniform(0.2, 0.7), 2)  # THE TRAP (Randomized severity)
            m3 = round(rng.uniform(1.2, 1.6), 2)  # Afternoon recovery
            m4 = round(rng.uniform(0.9, 1.1), 2)  # Evening normal

            customers = []
            total_demand = 0
            max_cust_demand = 0
            for i in range(1, num_cust + 1):
                res = gen_customer(rng, layout, i, num_cust)
                customers.append(res)
                total_demand += res[2]
                max_cust_demand = max(max_cust_demand, res[2])

            avg_required_cap = (total_demand / total_k) * 1.3
            truck_capacities = []
            for _ in range(total_k):
                multiplier = rng.triangular(0.6, 1.0, 2.0) 
                cap = int(avg_required_cap * multiplier)
                cap = max(cap, max_cust_demand + 5)
                truck_capacities.append(cap)
            
            truck_capacities.sort()

            name = f"TD_UNIQUE_{layout}_{num_cust}_{file_id:03d}"
            f.write(f"INSTANCE {name}\n")
            f.write(f"LAYOUT {layout}\n")
            f.write(f"CUSTOMERS {num_cust}\n")
            f.write(f"TOTAL_VEHICLES {total_k}\n")
            
            f.write(f"VEHICLE_CAPACITIES\n")
            f.write(" ".join(map(str, truck_capacities)) + "\n")

            # Dynamic Intervals
            f.write("M_INTERVALS 4\n")
            f.write("START    END      SPEED_MULTIPLIER\n")
            f.write(f"0        300      {m1}\n")
            f.write(f"300      600      {m2}\n") 
            f.write(f"600      1000     {m3}\n")
            f.write(f"1000     2000     {m4}\n")

            f.write("CUSTOMER_DATA\n")
            f.write("CUST_NO  XCOORD  YCOORD  DEMAND  READY_TIME  DUE_DATE  SERVICE_TIME\n")
            f.write(f"0        50      50      0       0           {depot_due}       0\n")

            for i, (x, y, demand, ready, due, service) in enumerate(customers, 1):
                due = min(due, depot_due - service - 30)
                if ready >= due: ready = max(0, due - 40)
                f.write(f"{i:<8} {x:<7} {y:<7} {demand:<7} {ready:<11} {due:<9} {service}\n")
            f.write("\n")

    print(f"Generated {NUM_DATASETS} Unique-Fleet instances with Random Traffic in {filename}")

if __name__ == "__main__":
    generate_all_datasets()