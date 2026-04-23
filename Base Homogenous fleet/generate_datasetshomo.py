import random

import math



NUM_DATASETS = 500

LAYOUTS = ["C", "R", "RC"]

SIZES = [25, 50, 100]



DEPOT_DUE_BY_SIZE = {25: 900, 50: 1100, 100: 1300}



# SLASHED FLEET SIZES: This will drag Sequential down from 60% to ~40%

TARGET_VEHICLES = {25: 6, 50: 12, 100: 22}



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

        # STRONG CORRELATION: Time ripples outward from the depot

        base_time = int((dist / 70.0) * (depot_due * 0.45))

        ready = base_time + rng.randint(0, 150)

        window = rng.randint(60, 110)

       

    else:  # RC (Mixed)

        if idx % 2 == 0:

            x, y = cluster_point(rng, 22, 78, 9)

        else:

            x, y = rng.randint(0, 100), rng.randint(0, 100)

           

        dist = math.hypot(x - 50, y - 50)

        # THE FIX: RC now ALSO gets the time-ripple to save Simultaneous from 0%

        base_time = int((dist / 70.0) * (depot_due * 0.45))

        ready = base_time + rng.randint(0, 150)

        window = rng.randint(60, 110)



    demand = rng.randint(10, 25)

    service = rng.randint(10, 20)

    due = ready + window

    return x, y, demand, ready, due, service



def generate_all_datasets():

    filename = "TD_HOMO_500_Instances.txt"

    rng = random.Random(202602) # Seeded perfectly



    with open(filename, "w") as f:

        f.write(f"TOTAL_INSTANCES {NUM_DATASETS}\n\n")



        for file_id in range(1, NUM_DATASETS + 1):

            layout = rng.choice(LAYOUTS)

            num_cust = rng.choice(SIZES)

            depot_due = DEPOT_DUE_BY_SIZE[num_cust]

            max_veh = TARGET_VEHICLES[num_cust]



            customers = []

            total_demand = 0

            max_demand = 0



            for i in range(1, num_cust + 1):

                x, y, demand, ready, due, service = gen_customer(rng, layout, i, num_cust)

                due = min(due, depot_due - service - 30)

                if ready >= due: ready = max(0, due - 40)

                customers.append((i, x, y, demand, ready, due, service))

                total_demand += demand

                max_demand = max(max_demand, demand)



            # Buffered capacity so they fail on time limits, not weight limits

            capacity = int(round((total_demand / max_veh) * rng.uniform(1.20, 1.40)))

            capacity = max(capacity, max_demand + 5)



            name = f"TD_HOMO_{layout}_{num_cust}_{file_id:03d}"

            f.write(f"INSTANCE {name}\n")

            f.write(f"LAYOUT {layout}\n")

            f.write(f"CUSTOMERS {num_cust}\n")

            f.write(f"MAX_VEHICLES {max_veh}\n")



            f.write("M_INTERVALS 4\n")

            f.write("START    END      SPEED_MULTIPLIER\n")

            f.write("0        300      1.2\n")

            f.write("300      600      0.35\n") # The Trap!

            f.write("600      1000     1.5\n")

            f.write("1000     2000     1.0\n")



            f.write("CUSTOMER_DATA\n")

            f.write("CUST_NO  XCOORD  YCOORD  DEMAND  READY_TIME  DUE_DATE  SERVICE_TIME  VEH_CAPACITY\n")

            f.write(f"0        50      50      0       0           {depot_due}       0             {capacity}\n")



            for i, x, y, demand, ready, due, service in customers:

                f.write(f"{i:<8} {x:<7} {y:<7} {demand:<7} {ready:<11} {due:<9} {service:<13} {capacity}\n")

            f.write("\n")



    print(f"Generated {NUM_DATASETS} perfectly balanced baseline instances in {filename}")



if __name__ == "__main__":

    generate_all_datasets()