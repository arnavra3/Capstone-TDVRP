<div align="center">
  
# 🚚 Time-Dependent Multi-Trip Vehicle Routing Problem (TD-MT-VRP)
**A Simultaneous Heuristic Approach for Heterogeneous Fleets in Dynamic Traffic**

[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![Optimization](https://img.shields.io/badge/Optimization-Operations%20Research-success.svg)]()
[![Status](https://img.shields.io/badge/Status-Complete-brightgreen.svg)]()

*A Capstone Research Project evaluating the trade-offs between computational time, fleet minimization, and dynamic reload mechanics across NP-Hard logistics networks.*

---
</div>

## 📌 Executive Summary
Traditional vehicle routing assumes constant travel speeds and infinite truck capacities. Real-world logistics do not. [cite_start]This project introduces a highly realistic, C++ based routing engine that assigns customers to specific vehicles and routes them to minimize total time spent[cite: 4]. It solves for three massive real-world complexities simultaneously: Time-Dependent Traffic, Heterogeneous Fleets, and Multi-Trip Dynamic Returns.

---

## 🛑 1. The Problem Formulation

[cite_start]The problem is modeled as a complete directed graph $G(V,E)$, where nodes represent physical locations (the central depot and customers) and edges represent the road network[cite: 10, 12]. [cite_start]A fleet of vehicles with fixed, varied capacities must serve customers with specific demands[cite: 3].

### The Core Complexity: The Time-Dependent Step Function
The defining challenge of the TDVRP is that travel time is not based solely on physical distance. [cite_start]The travel time between two customers depends on the time of day the truck starts driving[cite: 5]. 

[cite_start]To model this mathematically, the day is divided into distinct time intervals ($M$)[cite: 14, 24]. [cite_start]The travel time $c(t)$ is calculated as a known step function based on the departure time $t$ at the origin node[cite: 13]. [cite_start]Once the departure interval is known, the transit time becomes a fixed constant for that link[cite: 15].

**Visualizing the Step Function:**
*If a truck leaves Node A for Node B at 8:00 AM (Rush Hour), it falls into Interval 1. If it waits until 11:00 AM, it falls into Interval 2.*

```mermaid
%%{init: {'theme': 'base', 'themeVariables': { 'primaryColor': '#f4f4f4', 'edgeLabelBackground':'#ffffff', 'tertiaryColor': '#ffffff'}}}%%
graph LR
    A[Node i<br>Origin] -->|Interval 1: Peak Traffic<br>Travel Time = 50 mins| B[Node j<br>Destination]
    A -->|Interval 2: Normal Traffic<br>Travel Time = 30 mins| B
    A -->|Interval 3: Light Traffic<br>Travel Time = 20 mins| B
    
    style A fill:#e1f5fe,stroke:#01579b,stroke-width:2px
    style B fill:#e8f5e9,stroke:#1b5e20,stroke-width:2px
