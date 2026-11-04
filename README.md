# AeroOS

A local operating systems project with four experiment areas. The OS algorithms run in C; Python connects the browser to C and stores results; React displays the returned state.

This project was developed with AI assistance. Removing unused files does not change that history. Follow your course's rules about assistance, authorship and what you must implement yourself.

## Start the project

Use the single project folder `/Users/rahinivarma/Desktop/os`:

```sh
cd /Users/rahinivarma/Desktop/os
./start.sh
```

Open http://127.0.0.1:5050. Keep that terminal running; Ctrl+C stops the app. The backend entry point is `backend/app.py`; there is no root `app.py`.

On a fresh computer, install Apple Command Line Tools, Python 3.10+, Node 20.19+ or 22.12+ and npm. The startup script creates the Python environment. Run `./build.sh` to build the frontend; its first run installs missing frontend dependencies. Build the native executable from the C sources with `make -C core`.

## The four feature areas

| Area | Important behavior |
|---|---|
| Airport | Run a seeded scenario, inspect actual resource owners, inject supported faults, replay recorded snapshots. |
| Scheduler | Compare nine policies on the same CPU workload and run paired what-if experiments. |
| Safety lab | Explore finite acquisition plans, replay shortest deadlock traces, compare the safety shield, run Banker requests and recovery. |
| Workbench | Compare memory policies and inspect recorded real POSIX-thread experiments. |

## Where the code lives

| File or folder | Responsibility |
|---|---|
| `core/src/main.c` | Read a JSON request and select one native command. |
| `core/src/json.c`, `flight.c`, `core/include/aero.h` | JSON access, process records and shared definitions. |
| `core/src/scheduler.c` | Simulated clock, nine policies, events, snapshots and measured timing. |
| `core/src/resource_manager.c` | Resource capacity, compatibility, ownership and release. |
| `core/src/banker.c` | Maximum-claim safety, current-request deadlock detection and recovery. |
| `core/src/memory_manager.c` | Contiguous allocation, compaction, swap-out and page replacement. |
| `core/src/synchronization.c` | Real pthread experiments, locks, semaphores and joined event records. |
| `core/src/model_checker.c` | BFS over finite acquisition plans, visited states, shortest traces and shield comparison. |
| `backend/app.py` | HTTP routes, input validation and fixed C subprocess calls. |
| `backend/database.py`, `scenarios.py` | SQLite persistence and input presets. |
| `frontend/src/App.tsx`, `store.tsx` | Shared scenario/run state, page selection and playback. |
| `frontend/src/Airport.tsx`, `Scheduler.tsx`, `Safety.tsx`, `Workbench.tsx` | One file per active feature area. |
| `frontend/src/components.tsx` | Only the four shared components actually used: Gantt, FlightTable, Modal and ResourceGraph. |
| `frontend/src/Desktop.tsx`, `styles.css`, `types.ts`, `main.tsx` | Folder navigation, visual styles, data types/API helper and browser entry point. |
| `tests/` | Native assertions, engine tests, API/persistence tests and independent verifier oracle. |
| `data/` | Saved SQLite records and reproducible scenario/result files. |

## Follow one request

1. A feature page calls the API helper in `types.ts`.
2. A route in `backend/app.py` checks the request and calls `engine` with a fixed command.
3. `main.c` selects the appropriate C module. C returns JSON rather than drawing the interface.
4. `database.py` archives the request and result when the route uses persistence.
5. React displays the returned records. Replay changes a snapshot index; it does not reverse a running C process.

For the advanced feature, read `model_checker.c` in this order: state representation, allocation calculation, safety test, BFS exploration, trace reconstruction and input bounds. Try capacities [1,1,1] with plans [0,1], [1,2], [2,0]. The baseline has 45 states and a three-acquisition deadlock trace; the shielded model has 44 states and no reachable deadlock when exploration is exhaustive. Be able to explain why a successful witness alone is not a proof, and why reaching the state limit means inconclusive.

## Verify the project

```sh
./tests/run_all.sh --sanitize
```

The suite checks native resource ownership, scheduling, memory, synchronization, API persistence and the model checker, followed by the frontend build. The Python verifier oracle is testing code; the product checker runs in C. Sanitizer runs repeat native tests with instrumentation rather than adding independent test designs.

- [Current test evidence](docs/test-results-2026-10-29.txt)
- [Algorithm rules](docs/algorithms.md) and [architecture](docs/architecture.md)
- [Finite model contract and limits](docs/beyond-syllabus.md)
- [Demo](docs/demo.md), [review questions](docs/viva.md) and [test coverage](docs/testing.md)
- [Original requirement audit](docs/audit-2026-10-26.md)

Flights represent one service interval, thread logs are inspected after workers join, and swapping is swap-out only. The model checker verifies declared finite resource plans, not the airport engine, arbitrary C or real host scheduling. The complete original wish list is not implemented; the audit records partial features.

## Cleanup recovery

Unused legacy components and unused chart/animation dependencies were removed. A duplicate extracted project, old reports, stale screenshots and report-generation intermediates were moved outside this folder to `/Users/rahinivarma/Desktop/AeroOS cleanup archive 2026-10-29`. That archive also preserves the duplicate copy's database and a source snapshot from before cleanup. The root `data/` records and `.git` history were not deleted.

## Interface simplification

The Files section, Saved files shortcut and Save position control were removed from the website on 29 October 2026. Airport, Scheduler, Safety lab and Workbench remain. Stored data and backend persistence are retained. A recovery copy of the removed interface is outside the project in the cleanup archive.
