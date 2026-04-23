# Time-Dependent Vehicle Routing Problem (TDVRP)

This project implements a routing engine to solve the Time-Dependent Vehicle Routing Problem (TDVRP). The TDVRP is an advanced operations research method used to compute optimal delivery routes for a fleet of vehicles. It works by constructing a directed network of customers and a central depot, evaluating vehicle capacities against customer demands, and generating routes that strictly minimize the total time spent by the entire fleet.

---

## The Time-Dependent Constraint

* **Distance:** The physical distance between nodes is fixed.
* **Intervals:** The operational day is divided into distinct time periods ($m$).
* **Step-Function:** Travel time depends entirely on departure time $t$.

Unlike standard routing models, the travel time between two nodes is not a static number. The day is divided into traffic intervals (e.g., morning rush hour, afternoon lull). A vehicle departing a node during a peak interval incurs a significant time penalty. This forces the algorithm to balance geographic efficiency against the heavy cost of traffic congestion.

---

## 1. Network Initialization

We create a complete directed graph `G(V, E)` consisting of `n` nodes (representing the depot and customers). The network relies on a time-dependent matrix `C(t)`. Every edge `(i, j)` is evaluated across `m` parallel time intervals. 

The central depot is treated strictly as a "Start Only" node `0`. We initialize the routing sequence by setting the fleet's starting clock to `T=0`, allowing the solver to accurately calculate arrival times, enforce vehicle capacity limits, and determine the exact traffic interval a truck will hit when traversing the next link.
