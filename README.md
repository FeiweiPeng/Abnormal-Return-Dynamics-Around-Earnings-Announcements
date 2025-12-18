# Abnormal Return Dynamics Analysis Around Earnings Announcements


## Overview

This project builds a **sector-neutral event study** on Russell 3000 stocks to measure how prices react around **earnings announcements**. Stocks are classified into **Beat / Meet / Miss** groups using a within-sector ranking approach, and their performance is evaluated relative to **IWV (Russell 3000 ETF)** as the market benchmark.

We estimate **Average Abnormal Returns (AAR)** and **Cumulative Average Abnormal Returns (CAAR)** using a **bootstrap-based resampling framework**, allowing us to assess both expected effects and statistical dispersion.

---

## Methodology

### 1. Sector-Neutral Grouping (Beat / Meet / Miss)

- For each industry sector, stocks are sorted by **earnings surprise**.
- The top 2% and bottom 2% are removed as outliers.
- The remaining stocks are split evenly into **Beat**, **Meet**, and **Miss** groups.
- Groups from all sectors are merged so each final group contains stocks from every sector, reducing sector bias.

---

### 2. Event Window and Return Definition

- **Day 0** is defined as the earnings announcement date.
- For each stock, **2N + 1** adjusted close prices are retrieved around the event (user-selected \(N \in [30, 60]\)).
- Stock log returns are computed as:

R_it = log(P_t / P_{t-1})

- Benchmark returns \(R_{mt}\) are computed from IWV on the same trading days.
- **Abnormal return** is defined as:

AR_it = R_it - R_mt

---

### 3. AAR and CAAR within One Bootstrap Sample

- Randomly sample **30 stocks** from each group (Beat / Meet / Miss).
- Compute abnormal returns for each stock.
- Average across stocks to obtain daily AAR:

AAR_t = (1 / M) * sum_{i=1}^M AR_it,  M = 30

- Cumulate AAR over time to form CAAR:

CAAR_T = sum_t AAR_t

---

### 4. Bootstrap Estimation (Expected Value and Dispersion)

- Repeat the sampling procedure **40 times** per group.
- This produces 40 AAR and CAAR paths for each group.
- Compute:
  - **Expected AAR / CAAR**: day-by-day mean across the 40 paths
  - **AAR-STD / CAAR-STD**: day-by-day standard deviation across the 40 paths

---

### 5. Output and Visualization

- Store summary statistics in a **3 × 4 matrix**:
  - Rows: Beat / Meet / Miss  
  - Columns: Expected AAR, AAR-STD, Expected CAAR, CAAR-STD
- Plot **expected CAAR curves** for all three groups using **gnuplot (X11)** for visual comparison.

---

## Interactive Menu & User Workflow

The program provides a **menu-driven interface** that allows users to run the analysis step-by-step and explore results interactively.

### Option 1 — Enter N and Pull Data
- User inputs the event window size **N (30–60)**.
- The program downloads **IWV benchmark prices** and **all stock price series** in parallel.
- Abnormal returns are computed, followed by **bootstrap sampling** and **statistical aggregation**.
- After completion, all statistics are stored and ready for display or plotting.

### Option 2 — Show Individual Stock Info
- User enters a stock ticker.
- The program displays detailed information including prices, cumulative returns, earnings data, and group assignment.
- Input validation ensures the program keeps prompting until a valid ticker is provided.

### Option 3 — Show Group Statistics
- User selects one group (**Beat / Meet / Miss**).
- A summary table is displayed showing:
  - Expected AAR  
  - AAR standard deviation  
  - Expected CAAR  
  - CAAR standard deviation  
- The user is then optionally prompted to view the **full time series** of AAR/CAAR statistics for that group.

### Option 4 — Plot Results
- Generates a **CAAR comparison plot** for all three groups using gnuplot.
- Allows visual inspection of post-earnings market reaction patterns.

### Option 5 — Exit
- Safely releases allocated resources and terminates the program.

---

## Project Structure
- `main.cpp` — Program entry and interactive menu
- `StockStructure.*` — Stock data container and return computation
- `StockUtils.*` — CSV parsing and trading-day alignment
- `StockGrouper.*` — Beat / Meet / Miss classification
- `Bootstrapper.*` — Bootstrap resampling logic
- `StatCalculator.*` — AAR / CAAR aggregation and reduction
- `MatrixOperator.*` — Matrix utilities
- `ThreadUtils.*` — Thread pool and rate-limiting
- `CurlUtils.*` — API data retrieval (libcurl)
- `Gnuplot.*` — Visualization interface
- `data/` — Input CSV files
- `Makefile`

---

## Build and Run
- make
- ./main
- Use the interactive menu to load data, query stocks, view group statistics, and generate CAAR plots.

---

## Authors
Team: Feiwei Peng, Yu Zhong, Mingjia Jin, Haochen Zou, Ting-Chen Chen

