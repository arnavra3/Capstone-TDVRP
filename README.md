# Time-Dependent Vehicle Routing Problem (TDVRP)
* **Heuristic Development:** Implementation of Sequential and Simultaneous dispatch architectures utilizing four decision rules: Shortest Travel Time (Baseline), Earliest Start Time, Earliest Finish Time, and Probabilistic Selection.
* **Large-Scale Validation:** Evaluation of success rates across 500 generated instances using three map topologies (Clustered, Random, and Mixed) to simulate diverse urban environments.
* **MILP Formulation:** Construction of a mathematical model for the Time-Dependent Vehicle Routing Problem (TDVRP) using the SCIP solver to establish exact performance baselines.
* **Novelty Benchmarking:** Comparative analysis between the novel time-centric heuristics (V2, V3, V4) and the foundational 1992 Malandraki & Daskin baseline (V1) to measure efficiency gains in dynamic traffic.
* **Computational Optimization:** Application of MILP pruning by injecting heuristic-derived lower bounds into the branch-and-bound process to accelerate the search for optimal proofs.
* **Micro-Benchmark Comparison:** Validation of heuristic accuracy against exact mathematical optima using scaled-down micro-benchmarks (n=4 to 10) to bypass NP-Hard time limits.
* **Multi-Trip Extension & Analysis:** Development of an MT-TDVRP variant incorporating a 30-minute warehouse reload penalty to analyze the operational tradeoff between fleet size reduction and total route time.
---

## Problem Formulation

The network is modeled as a complete directed graph `G(V, E)` consisting of `v` nodes (the physical customer locations plus the depot) and `e` links. In this model, the travel time between two points is not static; it depends on both the physical distance and the specific time of day the travel occurs.

### The Time-Dependent Step Function
The network utilizes an `N x N` time-dependent matrix `C(t) = [cij(t)]`. The travel time on link `(i, j)` is calculated as a step function of the departure time `t` at the origin node `i`. 

The operational day is divided into distinct time intervals (`M`). Once the departure time falls into a specific interval, the transit time for that link becomes a known constant. To represent this mathematically, the problem uses an expanded network where each physical link `(i, j)` is replaced by `M` parallel links, representing the varying traffic speeds across the day.

### Graph Transformation (The Sink Nodes)
To formulate this as a Mixed Integer Linear Programming (MILP) problem and completely eliminate mathematical routing loops, the depot architecture is modified into a one-way flow system:

* **The Source (Node 1):** The central depot is treated strictly as a "Start Only" point. All inbound links to this node are removed.
* **The Sinks (Nodes N+1 to N+K):** We introduce `K` virtual nodes to represent the "End of the Shift" for each of the `K` vehicles. 
* **One-Way Flow:** All vehicles start at Node 1 and must terminate at one of the unique return nodes. The calculated travel time to any sink node is exactly equal to the travel time to the original physical depot.

In this formulation, every node `i` has exactly one continuous variable `ti` representing the exact time the vehicle arrives at that node. 

### Core Assumptions
1. **Vehicle Independence:** The travel time across any interval `M` is independent of the vehicle type (a standard baseline for urban environments).
2. **Service Independence:** The collection or delivery time depends entirely on the customer, not the vehicle type.

### Visualizing the Graph Transformation (K=2 Vehicles)
To eliminate mathematical routing loops, the depot architecture is modified into a one-way flow system. The original depot becomes a "Start Only" node, and we generate virtual "End of Shift" sink nodes for every vehicle in the fleet.

---

## Mixed Integer Linear Programming (MILP) Model

The TDVRP is formulated as a Mixed Integer problem because it must simultaneously track discrete routing decisions (integer variables) and exact arrival schedules (continuous variables). 

By expanding the depot into `K` sinks, the objective function naturally minimizes the total route time across all vehicles:

<img width="167" height="60" alt="image" src="https://github.com/user-attachments/assets/d4b98896-d2db-4d28-8f6e-399699a8c452" />

The complete formulation enforces strict constraints for node visitation, capacity limits, flow conservation, and the time-dependent link traversal:

<img width="901" height="847" alt="image" src="https://github.com/user-attachments/assets/e6e3826c-128b-43ea-8701-46a0b07c7ef2" />

---

## Heuristic Methodologies

Because exact mathematical proofs fail on realistically sized networks, the engine implements two algorithmic architectures to construct feasible routes efficiently.

### Route Construction Architectures
* **Sequential Dispatch (SEQ):** A local greedy approach. It deploys a single vehicle, assigning it customers until it hits a capacity or time limit, and only then wakes a new truck from the depot.
* **Simultaneous Dispatch (SIM):** A global greedy approach. At every step, it evaluates all unvisited customers against *all* currently active trucks plus available garage trucks, prioritizing system-wide speed over fleet minimization.

### Different Heuristics (The 4 Variants)
To determine the "best" move during construction, four distinct evaluation logics were implemented. Variant 1 acts as the academic baseline, while Variants 2, 3, and 4 represent novel extensions developed for this project.

1. **V1: Shortest Travel Time (Baseline):** A pure greedy approach optimizing only for driving duration. Often fails because trucks rush to closed time windows and are forced to sit idle.
2. **V2: Earliest Start Time (Novel):** Shifts optimization to arrival time. Reduces gate idling but ignores service duration, frequently trapping trucks in peak traffic upon departure.
3. **V3: Earliest Finish Time (Novel):** Optimizes for absolute completion (Travel + Wait + Service). Naturally prioritizes nearby customers who are ready for immediate processing.
4. **V4: Probabilistic Selection (Novel):** Looks at the top 5 options of the Earliest Finish Time and randomly chooses one. It repeats this process over 20 iterations and picks the highest-performing outcome to escape local optima.

---

### 1) Base Fleets

We implemented all four heuristics on a total of 500 instances across three map structures (Clustered, Random, and Mixed) to simulate diverse urban and suburban environments. To strictly test the time-dependency, travel velocities were subjected to 4 distinct time intervals across the operational day.

**i) Base Homogeneous Fleets :**
The initial benchmarking phase assumed a depot of identically sized vehicles. We applied the 4 heuristics and evaluated the success percentage of finding a valid solution across all three mapped methodologies. This isolated the time-dependent routing logic to prove it could navigate traffic constraints before introducing capacity variance.

**ii) Base Heterogeneous Fleets :**
The fleet constraint was then upgraded to include vehicles of varying capacities. We applied all 4 heuristics using the following dispatch strategies:

* **SEQ-LS (Sequential Large-to-Small):** This strategy uses the highest capacity vehicles first to clear dense, high-demand clusters before the traffic trap hits.
* **SEQ-SL (Sequential Small-to-Large):** This efficiency strategy saves larger demand for the end of the day, using small trucks for morning deliveries to minimize wasted capacity.
* **SIM (Simultaneous Best-Fit):** A hybrid of bin-packing and time-greedy logic. We looked at all vehicles and all customers simultaneously. For each customer, the algorithm finds the smallest garage truck that fits their demand (Best-Fit). Those Best-Fit options are put into a global pool with all other active trucks. The entire pool is then sorted by Earliest Finish Time (V3).

---

### 2) Benchmark with MILP Implementation

To rigorously evaluate the heuristics against exact mathematical proofs, a set of challenging benchmark instances was generated. The parameters closely mirror the methodology established in the foundational **Malandraki & Daskin (1992)** paper, but with extremely tight capacity constraints to stress-test the algorithms.

**Benchmark Generation Parameters:**
* **Network Size ($n$):** Small-scale networks consisting of 10, 15, 20, and 25 customers.
* **Time Intervals ($m$):** 2 or 3 distinct traffic periods per operational day.
* **Travel & Service Times:** Base travel times randomized [20, 80] and mapped to a $100 \times 100$ coordinate grid; service (unloading) durations randomized between [10, 20] minutes.
* **Strict Capacity Constraints:** Fleet size restricted to between $n/6$ and $n/2$. Capacities were tightly calibrated so that the total fleet capacity exceeded total network demand by **exactly 10%**, forcing the algorithms to perform highly efficient bin-packing to survive.

#### The 10-Minute Timeout & The `< 1.00` Ratio
These generated instances were fed into an exact SCIP MILP solver. Because the TDVRP is NP-Hard, the exact solver was capped at a **10-minute (600s) time limit** per instance. For nearly all networks where $n \ge 15$, the solver failed to find the optimal proof within the limit, returning only a "feasible at best" solution.

We evaluated the algorithms using a `Heuristic Time / MILP Best Time` ratio. Theoretically, an optimal ratio should always be $\ge 1.00$. However, our results frequently yielded outputs of **`< 1.00`**. 

This anomaly occurred because the custom C++ heuristics successfully discovered a *faster, more efficient route in mere seconds* than the exact mathematical solver could find in 10 full minutes of computation. Most notably, the novel heuristics developed for this project (**V2, V3, and V4**) consistently and significantly outperformed the baseline **V1 (Travel Time)** heuristic provided in the original 1992 paper.
<table>
  <tr>
    <td align="center">
      <img src="https://github.com/user-attachments/assets/784b5887-0b21-4730-b95c-3d55771c39d0" alt="MILP Results 1" width="100%" />
    </td>
    <td align="center">
      <img src="https://github.com/user-attachments/assets/50143490-79e8-4041-a3df-d3ba71657731" alt="MILP Results 2" width="100%" />
    </td>
  </tr>
</table>

---

### 3) MILP Optimization by Pruning using Heuristic Solution

To address the `< 1.00` ratio anomaly and force a mathematically valid baseline comparison, an objective bounding (pruning) architecture was integrated into the MILP solver. The goal was to assist the exact solver in finding an optimal proof faster by reducing its search space.

**The Pruning Mechanism:**
1. **Global Minimum Extraction:** The system evaluated the output CSVs from all executed heuristic variants (SEQ-LS, SEQ-SL, and SIM) for a given instance.
2. **Objective Bounding:** The absolute minimum (fastest) route time discovered among all heuristics was extracted and fed directly into the SCIP MILP solver as a strict upper bound. 
3. **Branch-and-Bound Acceleration:** During execution, if the MILP solver explored a mathematical routing path that resulted in a worse time than the heuristic bound, that entire branch was immediately pruned. 

**Results and Computational Reality:**
While this technique theoretically speeds up the solver by drastically shrinking the search space, it yielded limited success under the strict 10-minute (600s) time constraint. 

It frequently timed out without returning *any* feasible solution (as all feasible paths it discovered in 10 minutes were worse than the heuristic prune point and thus discarded). If there was a longer time allowed for the MILP then we could find better results.

<img width="400" height="250" alt="image" src="https://github.com/user-attachments/assets/c434afd7-8b94-4383-8742-4d0a06ddaf7c" />

---

### 4) Micro-Benchmark Comparisons

Because the standard networks universally timed out the exact solver, a specialized set of micro-benchmarks was generated to mathematically validate the efficiency of the heuristics. 

**The Micro-Scale Parameters:**
* The network size was scaled down to $n = 4, 6, 8, 10$ customers (reduced from $10-25$).
* All other generation parameters—including the dynamic traffic intervals, the $100 \times 100$ coordinate grid, and the strict 10% capacity margin—remained completely identical.

**Results Against Exact Proofs:**
We re-applied the SCIP MILP solver with the same 10-minute limit. Because the factorial state space was significantly smaller, the solver successfully reached the true mathematical optimal bounds for these instances. This provided a 100% valid baseline to evaluate the heuristics.

The results explicitly proved that our novel time-centric algorithms (**V2, V3, and V4**) dramatically outperformed the standard distance-greedy **V1** baseline from the 1992 paper. Optimizing for "Earliest Finish Time" (V3/V4) successfully navigated the time-dependent traffic traps that caused the legacy V1 algorithm to fail, generating near-optimal routes in a fraction of the computational time.

<table>
  <tr>
    <td align="center"><img src="https://github.com/user-attachments/assets/990f5d6a-4a3d-48c0-aa80-0771a6ce0846" alt="Micro-Benchmark Result 1" width="100%" /></td>
    <td align="center"><img src="https://github.com/user-attachments/assets/c31fd86d-5833-45a1-bf8d-213ef32d2ce9" alt="Micro-Benchmark Result 2" width="100%" /></td>
  </tr>
  <tr>
    <td align="center"><img src="https://github.com/user-attachments/assets/2ab267b2-d9ec-42ee-b86e-8247db8cd2c4" alt="Micro-Benchmark Result 3" width="100%" /></td>
    <td align="center"><img src="https://github.com/user-attachments/assets/8dcc7274-a06a-4e87-b05d-d0ed27ef3227" alt="Micro-Benchmark Result 4" width="100%" /></td>
  </tr>
</table>

---

### 5) Multi-Trip TDVRP (MT-TDVRP)

To model maximum operational efficiency, we implemented a Multi-Trip extension where every vehicle is allowed to perform multiple routing shifts to satisfy customer demands. 

Instead of permanently retiring a vehicle once its capacity is exhausted, the algorithm now generates a dynamic "Return to Depot" command. 

**The Reload Logic:**
* **The Penalty:** The vehicle drives back to the central depot, drops its loaded volume back to zero, and incurs a strict 30-minute Reload Penalty to realistically simulate warehouse loading times.
* **The Time Check:** Before heading back out for a second shift, the truck checks its internal clock. If the current time plus the travel time to the next customer violates the depot's end-of-day closing deadline, the truck is officially retired for the day.
* **Penalty Rollback:** If a truck returns to the depot and triggers the 30-minute reload penalty, but the algorithm then realizes it is too late in the day to make any further deliveries, the 30-minute penalty is mathematically rolled back.

To maintain a valid comparison, the exact same micro-benchmark instances were used to test the MT-TDVRP.

---

#### The Multi-Trip Tradeoff (Time vs. Fleet Size)

Allowing multiple trips introduces a massive strategic tradeoff between the total time spent driving (Heur/Best ratio) and the size of the fleet required. The two routing architectures handled this dynamic return mechanic very differently:

* **Sequential Dispatch (SEQ):** Because the SEQ heuristic focuses on fully utilizing one vehicle before waking another, it aggressively leverages the Multi-Trip mechanic. It will force a vehicle to return, reload, and deploy until its shift clock completely expires. As a result, the **number of vehicles used drops significantly**, but the total route time (Heur/Best ratio) increases slightly due to the repeated 30-minute warehouse penalties.
* **Simultaneous Dispatch (SIM):** As a global greedy algorithm, SIM sorts every possible move strictly by Earliest Finish Time (V3). Because a 30-minute reload penalty severely hurts a truck's "Finish Time" score, the SIM algorithm actively resists using the Multi-Trip feature unless it is absolutely forced to. It checks the normal move as well as the reload move, but generally prefers deploying a fresh garage truck. Consequently, for the SIM heuristic, the fleet size and route times remain almost identical to the single-trip version.

Ultimately, this tradeoff allows logistics companies to select an algorithm based on their operational priorities: minimizing their active fleet size (SEQ) or strictly minimizing total delivery time (SIM).

<table>
  <tr>
    <td align="center"><img src="https://github.com/user-attachments/assets/bc73cece-fa00-497e-962f-8aa20d8fee2c" alt="Multi-Trip Result 1" width="100%" /></td>
    <td align="center"><img src="https://github.com/user-attachments/assets/d59ff188-559b-45e6-b727-7ed84d05a58a" alt="Multi-Trip Result 2" width="100%" /></td>
  </tr>
  <tr>
    <td align="center"><img src="https://github.com/user-attachments/assets/661bf70e-177f-43a7-b8da-bb8aa40125f4" alt="Multi-Trip Result 3" width="100%" /></td>
    <td align="center"><img src="https://github.com/user-attachments/assets/41d9908b-dd32-459b-b14b-2f350ef6cbda" alt="Multi-Trip Result 4" width="100%" /></td>
  </tr>
</table>









