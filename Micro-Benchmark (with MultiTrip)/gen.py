import random
import math

# MICRO-BENCHMARK SIZES: Scaled down to guarantee SCIP optimality < 10 mins
SIZES = [4, 6, 8, 10] 
INTERVAL_TYPES = [2, 3]
REPLICATIONS = 4 # 4 layouts per size/interval combo
NUM_DATASETS = len(SIZES) * len(INTERVAL_TYPES) * REPLICATIONS # Equals 32

def generate_malandraki_32_micro():
    # Renamed output file so you don't overwrite your large instances
    filename = "TD_MALANDRAKI_32_Micro_Instances.txt"
    rng = random.Random(2026) # Fresh seed for new layouts

    with open(filename, "w") as f:
        f.write(f"TOTAL_INSTANCES {NUM_DATASETS}\n\n")
        
        file_id = 1
        
        for num_cust in SIZES:
            for num_intervals in INTERVAL_TYPES:
                for rep in range(1, REPLICATIONS + 1):
                    
                    # Fleet Size Rule: Uniform [N/6, N/2]
                    min_k = max(2, int(num_cust / 6))
                    max_k = max(2, int(num_cust / 2))
                    total_k = rng.randint(min_k, max_k)

                    if num_intervals == 2:
                        intervals = [(0, 300, 1.2), (300, 1000, 0.6)]
                    else:
                        intervals = [(0, 200, 1.2), (200, 400, 0.4), (400, 1000, 1.0)]

                    customers = []
                    total_demand = 0
                    max_cust_demand = 0
                    
                    for i in range(1, num_cust + 1):
                        x, y = rng.randint(0, 100), rng.randint(0, 100)
                        demand = rng.randint(10, 30)
                        service = rng.randint(10, 20)
                        
                        # Generous time windows to prevent "Infeasible" errors
                        ready = rng.randint(0, 150)
                        due = ready + rng.randint(250, 500)
                        
                        customers.append((x, y, demand, ready, due, service))
                        total_demand += demand
                        max_cust_demand = max(max_cust_demand, demand)

                    # Capacity Generation Rule (Equations 20/21)
                    avcap = (total_demand / total_k) * 1.2 
                    avcap = max(avcap, max_cust_demand + 5)
                    
                    truck_capacities = []
                    for _ in range(total_k):
                        upper_bound = max(max_cust_demand + 1, int((2 * avcap) - max_cust_demand))
                        cap = rng.randint(int(max_cust_demand), upper_bound)
                        truck_capacities.append(cap)
                    
                    # Sort capacities Large to Small
                    truck_capacities.sort(reverse=True)

                    name = f"TD_MALANDRAKI_MICRO_N{num_cust}_INT{num_intervals}_REP{rep}"
                    f.write(f"INSTANCE {name}\n")
                    f.write(f"LAYOUT R\n")
                    f.write(f"CUSTOMERS {num_cust}\n")
                    f.write(f"TOTAL_VEHICLES {total_k}\n")
                    
                    f.write(f"VEHICLE_CAPACITIES\n")
                    f.write(" ".join(map(str, truck_capacities)) + "\n")

                    f.write(f"M_INTERVALS {num_intervals}\n")
                    f.write("START    END      SPEED_MULTIPLIER\n")
                    for start, end, speed in intervals:
                        f.write(f"{start:<8} {end:<8} {speed}\n")

                    f.write("CUSTOMER_DATA\n")
                    f.write("CUST_NO  XCOORD  YCOORD  DEMAND  READY_TIME  DUE_DATE  SERVICE_TIME\n")
                    f.write(f"0        50      50      0       0           1000      0\n")

                    for i, (x, y, demand, ready, due, service) in enumerate(customers, 1):
                        f.write(f"{i:<8} {x:<7} {y:<7} {demand:<7} {ready:<11} {due:<9} {service}\n")
                        
                    f.write("\n")
                    file_id += 1

    print(f"Generated exactly {NUM_DATASETS} Micro instances in {filename}")

if __name__ == "__main__":
    generate_malandraki_32_micro()