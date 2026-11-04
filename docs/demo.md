# AeroOS: explanation, live demo and viva study guide

Prepared from the current source, existing test evidence and a browser walkthrough on 29 October 2026. This is a learning guide; use your own words and describe assistance honestly.

## 1. Understand the whole project

AeroOS is an educational OS workbench with an airport analogy. It runs in a browser, but its algorithms execute in a native C program. It is not a kernel, a bootable OS, or real aviation control software. The four folders are Airport, Scheduler, Safety Lab and Workbench. The Files interface was removed to simplify the demonstration; backend recording remains.

A flight represents a process. Arrival is when it enters the model; burst is how much service it needs; remaining service decreases while running. Priority helps select it. Its requested resources are equipment it must obtain before service. Its memory field is modeled memory demand. Runways, gates, baggage, maintenance, fuel and crew are six limited resource classes. A flight record resembles a process control block (PCB): identity, state, timing, priority, remaining work, memory and ownership information.

In Airport mode, service is one nonpreemptible interval. A flight acquires its entire compatible resource bundle and modeled memory together, holds them for that interval, then releases its own resources. This deliberately simplifies physical airport operations. All-or-nothing admission removes hold-and-wait in this model. Partial resource acquisition is studied separately in Safety Lab.

CPU mode is a separate abstraction: one CPU serves pausable jobs and allows preemptive policies. Do not say Round Robin interrupts a takeoff. Airport mode excludes those policies.

## 2. Technologies and their roles

| Technology | Why it is present |
|---|---|
| C17 and Clang | Native algorithms, explicit records, resource ownership, model checking and real concurrency experiments. |
| POSIX pthreads | Actual worker threads in the synchronization lab. |
| Mutexes and POSIX semaphores | Protect critical sections, bound concurrency and coordinate producers/consumers. |
| Python Flask | Local HTTP API, input validation, C invocation, error handling and report exports. |
| SQLite | Persistent scenarios, runs, events, metrics and saved replay positions. |
| React and TypeScript | Interactive browser screens with typed records and shared state. |
| Vite, Tailwind and custom CSS | Frontend build tooling and the paper-grid desktop appearance. |
| JSON | Structured data exchanged across both interfaces. |
| SVG | Resource maps, graphs and timeline visuals. |

There is no AI model training, machine learning dataset or cloud prediction service in the project. Workloads are authored or seeded synthetic inputs. Seed 42 gives repeatable generated scenarios. A seed reproduces inputs; it does not force the same host-thread schedule.

## 3. Architecture and request workflow

Browser React interface <-> JSON HTTP requests <-> Flask API <-> fixed native command with JSON standard input/output <-> C modules. Flask separately reads/writes SQLite.

1. The user chooses a workload, policy or lab input.
2. React collects the values and sends JSON to the appropriate API route.
3. Flask checks types, shapes, bounds and the allowed experiment kind.
4. Flask starts the fixed C executable with an allowed command. It passes JSON through standard input, captures standard output and enforces a timeout.
5. C runs the experiment and returns JSON containing measurements and traces.
6. Flask stores the original input and measured output in SQLite and returns the result with a run ID.
7. React displays the result. Airport playback selects already-recorded snapshots; it does not continuously stream a running airport process.

The API invokes a fresh C process per experiment. Flights themselves are simulated records, not individual operating-system processes or pthreads. Real pthreads exist in the separate synchronization experiment, and they are joined before the recorded result returns.

At each integer airport boundary, the engine applies timed capacities, completes service/releases owners, admits arrivals/applies cancellations, ranks eligible candidates, attempts compatible allocation and records a snapshot. During the following interval, remaining service decreases for running flights and waiting increases for ready/blocked flights. Backward replay selects a stored snapshot rather than undoing mutations.

## 4. OS concepts used

| OS concept | Where you can show it |
|---|---|
| Process records and state transitions | Airport flight inspector and process table. NEW, READY, RUNNING, BLOCKED, COMPLETED and CANCELLED. |
| CPU scheduling | Scheduler comparison, Gantt timeline and timing metrics. |
| Preemption and time quantum | Abstract CPU RR, SRTF, preemptive priority and MLFQ. |
| Resource allocation and mutual exclusion | Slot owners, compatibility checks and admission. |
| Deadlock prevention | Airport atomic bundles; dining experiment global resource ordering. |
| Deadlock avoidance | Banker and the model-checker safety shield. |
| Deadlock detection/recovery | Outstanding-request reduction followed by victim termination and owner release. |
| Threads, critical sections, mutexes and semaphores | POSIX threads workbench. |
| Producer-consumer and readers-writers | Bounded buffer and reader/writer experiments. |
| Paging and page replacement | FIFO, LRU, Optimal and address translation. |
| Contiguous memory and fragmentation | First/Best/Worst/Next Fit, freeing, compaction and modeled swap-out. |
| Interprocess communication | Flask exchanges JSON with a child C process through standard streams. |

SQLite persistence is an application feature, not an implementation of a complete OS file system. Booting, kernels, real hardware interrupts, virtualization and ROS are not implemented just because they appear in the syllabus.

## 5. Scheduling explained

FCFS chooses the earliest eligible arrival. SJF chooses the shortest burst among eligible jobs. SRTF chooses the shortest remaining time and may preempt an abstract CPU job. Priority chooses by priority; the preemptive variant may replace the current abstract job. Round Robin rotates jobs with a time quantum. HRRN chooses the greatest (waiting + burst) / burst. MLQ uses static priority bands. MLFQ moves CPU-consuming jobs through feedback queues with different allotments. Priority aging improves effective priority after accumulated ready waiting; enabling it does not change FCFS order.

For a completed job: turnaround = completion - arrival; waiting = turnaround - burst; response = first start - arrival. Throughput = completed jobs / elapsed simulated ticks. A tick is not a measured wall-clock second. Waiting includes ready and blocked intervals, including later waits after preemption. The implementation counts CPU process changes without simulating an explicit context-switch time penalty.

Fairness uses a Jain index over per-process completed service fractions (burst / turnaround). It is a defined project metric, not a universal fairness guarantee. Compare several metrics rather than claiming one policy always wins.

## 6. The beyond-syllabus contribution

The central extension is explicit-state model checking of finite resource acquisition plans, with shortest counterexample replay and a safety-shield comparison. Model checking is an established formal-methods technique; this project integrates it into an inspectable OS teaching workflow. Do not claim to have invented BFS, Banker or model checking.

A plan such as 0,1 means acquire resource R0, acquire R1, then release the bundle. A state stores how far every process has progressed through its plan. Held resources and free capacities are derived from these progress values. Each enabled next acquisition or release creates a transition. A packed state key deduplicates identical progress vectors reached by different orders.

Breadth-first search (BFS) explores transitions level by level. Parent pointers reconstruct the first deadlock found, giving a shortest failure in number of transitions. Deadlock means unfinished processes remain but no action is enabled. The checker also checks resource-capacity consistency.

The shield tentatively grants an acquisition and applies a maximum-claim Work/Finish safety reduction. It rejects a grant if it cannot establish a completion order. The checker separately explores the transitions retained by that shield.

Default model: capacities 1,1,1; plans P0:0,1; P1:1,2; P2:2,0. Baseline: 45 reachable states, 69 transitions and one deadlock. Shortest failure: P0 holds R0, P1 holds R1 and P2 holds R2; each waits for the next resource. Shield: 44 states, 66 transitions, zero deadlocks and three rejected unsafe grants. The search exhausts both reachable spaces.

A completion trace demonstrates one successful execution. The exhaustive state search supports the no-deadlock verdict. These are different forms of evidence. With a state limit of 1, the interface correctly says Search incomplete; zero deadlocks found in an incomplete search is not proof. Limits are six processes, four resources and six acquisitions per plan, with at most 50,000 stored states. More complex systems face state explosion. This does not verify the full airport, arbitrary C code or real host-thread fairness.

## 7. Repeatable website demonstration

Start from the project root /Users/rahinivarma/Desktop/os using ./start.sh and open http://127.0.0.1:5050/. The Flask entry point is backend/app.py, not a root-level app.py. Keep the startup terminal running.

### A. Airport: processes and resources

Open Airport and use Normal Airport Operations with FCFS. Step forward, play/pause, and move through the replay. Expand Flights & process states. Click a flight or runway to inspect identity, timing, state, memory, priority and ownership. At T+9 in the normal run, UK252 was RUNNING, arrival 6, remaining service 3, waiting 2 and response 2. Three runways were occupied; another flight was blocked.

Explain: an arrived flight can run only when its full compatible bundle and memory are available. Completion releases owned slots. Restart goes back through the recorded run.

Try a disruption -> Close one runway -> Inject creates a new branch. In the browser walkthrough, injection at T+8 announced a fault at T+9; the resulting run extended from 33 to 35 ticks. Existing owners finish; no active takeoff is forcibly interrupted. Other choices inject gate/fuel restrictions, an emergency arrival or a five-flight burst.

### B. Scheduler: policy tradeoffs

Open Scheduler -> Policy comparison -> Run comparison. The default comparison on the 14-flight workload produced:

| Policy | Mean waiting | First response | Switches | Fairness |
|---|---:|---:|---:|---:|
| FCFS | 26.57 | 26.57 | 13 | 0.52 |
| SJF | 18.93 | 18.93 | 13 | 0.74 |
| RR | 39.00 | 10.43 | 42 | 0.81 |
| MLFQ | 43.79 | 1.93 | 33 | 0.96 |

These are simulated ticks and depend on the chosen input. SJF had lower mean waiting here; MLFQ had faster first response. Choose policies to include the other algorithms, change the objective and click a result/Gantt block for details.

What-if experiment -> Add one runway -> Run paired experiment uses airport constraints. In this walkthrough, mean waiting went 1.14 -> 0.36, maximum waiting 4 -> 2, displayed throughput stayed 0.42 and failed allocation attempts went 16 -> 5. The displayed waiting delta was -0.79 because it is calculated before rounding the displayed averages. More capacity reduced waiting without changing the overall completion span in this workload.

Do not compare the airport average directly with the one-CPU average as if the hardware assumptions were the same.

### C. Safety Lab: show the advanced feature first if time is short

Interleaving explorer -> Explore interleavings. Read the baseline/shield cards. Select baseline step 3 to show all resources held and every process waiting. Select the shield card to see three rejected unsafe grants and the completion witness. Expand Exploration bound & guarantee, set the limit to 1 and rerun to show Search incomplete. Restore 20,000 and rerun for the complete result.

Banker tab: Available 3,3,2 with the supplied matrices. Submit the default P1 request 1,0,2. The browser showed Granted: resulting state is safe and sequence P1 -> P3 -> P4 -> P0 -> P2. Need = Maximum - Allocation. A tentative request must be within Need and Available; safe mode rolls back an unsafe result. Unsafe means completion cannot be guaranteed under maximum claims; it does not mean already deadlocked.

Deadlock recovery tab: the preset has P0 holding R0 and requesting R1, with P1 holding R1 and requesting R0. Detect & recover reported P0/P1 deadlocked, terminated P0 and restored progress. The detector uses current-request Work/Finish reduction, which handles multiple instances; a cycle alone is not enough in general. Recovery sacrifices a process; it is not a general rollback system.

### D. Workbench: memory

Memory -> Paging -> three frames -> Run & compare on references 7,0,1,2,0,3,0,4,2,3,0,3,2. Verified faults: FIFO 10, LRU 9, Optimal 7. LRU shows nine faults and six replacements because the first three faults fill empty frames. Move the trace slider to inspect frame contents. Address translation splits a logical address into page number and offset and uses the current frame mapping. Optimal uses future references, so it is an offline comparison baseline. The >70% thrashing label is a heuristic, not a working-set proof.

Select Contiguous allocation and run the default five operations. First and Best Fit showed 70 MB fragmentation; Worst and Next Fit showed 220 MB. Selected First Fit had 594 MB free split into 70 and 524 MB holes. The metric is total free space minus the largest hole. Freeing coalesces adjacent holes; compaction packs live blocks to combine free space. Add operations to show allocate/free/compact/swap-out. Swap only releases a modeled block; disk-backed swap-in is absent.

### E. Workbench: real threads

POSIX threads -> four threads and 20 iterations -> mutex -> Run threads: all four joined, 80 updates, peak critical-section occupancy 1. The lock protects read-modify-write updates.

Switch to race and rerun: expected 80, observed 20. Coordinated atomic loads/stores and barriers make lost updates visible without introducing undefined C data races. Individual atomic accesses do not make the whole load/add/store sequence atomic.

Switch to producer_consumer: the browser showed 40 consumed items. Two producers and two consumers each perform 20 operations. Empty/full semaphores coordinate a bounded 16-item circular queue; a mutex protects its contents and indices.

Other implemented modes: binary semaphore permits one worker; counting semaphore permits up to its capacity (default 2); readers-writers coordinates readers with exclusive writes; dining uses a global fork acquisition order to eliminate circular wait. These other four modes are covered by the existing tests; this walkthrough specifically reran mutex, race and producer-consumer. Thread event ordering may change across runs because these are actual host threads. The displayed event slider inspects a completed recorded trace.

### Recorded evidence

Flask still archives experiment inputs and results in SQLite. The Files screen, scenario-management controls and saved-position controls are no longer part of the website demonstration.

## 8. Teacher questions: answers to understand

**What did you learn?** Explain process state transitions, scheduling tradeoffs, resource ownership, how unsafe allocation differs from deadlock, synchronization of real threads, memory policy differences, and why reproducible testing needs controlled inputs. The advanced lesson is that one successful run cannot establish that every interleaving is safe. Mention only what you can explain yourself.

**Why an airport?** It makes limited resources and waiting visible through familiar examples. It is a teaching analogy, not a physically complete airport model.

**What is new beyond class?** Finite-state interleaving exploration, deduplication, shortest failure replay, shielded-versus-unprotected verification and honest inconclusive results under bounds. The class algorithms support this workflow; they are not claimed as new inventions.

**Why C, Python and React?** C owns algorithm execution and POSIX experiments; Python integrates, validates and persists; React presents interactive results. Each layer can be inspected independently.

**Why not one thread per flight?** Host scheduling would obstruct deterministic comparisons. Airport simulation uses controlled integer boundaries; the separate lab demonstrates actual concurrency.

**What are the advantages?** Visible process/resource states; repeatable seeded workloads; controlled comparisons; inspectable failure traces; persistent evidence; real synchronization experiments; native modules tested independently from the UI.

**What are the disadvantages?** Simplified one-bundle airport service; synthetic inputs; no real hardware/airport connection; finite model/state explosion; known maximum claims required by the shield; recorded playback rather than live streaming; no model-checking proof of the whole application; no complete disk-backed swapping or OS file system; no general starvation or real-time guarantee.

**How do you know it works?** Existing final evidence records 49 tests: 34 engine, nine verifier and six API tests, plus a separate native resource assertion harness. ASan/UBSan repeated the 34+9 native-facing suites; these are repeated checks, not 43 additional independent test designs. Verifier tests include an independent Python oracle with 35 seeded subcases. TypeScript checking and the production build also passed. This walkthrough verified the listed browser outcomes; it did not repeat the entire automated suite.

**Can you claim everything is safe?** No. State precisely which finite resource model was fully explored and which test cases passed.

## 9. Explain the academic report

Submission file: reports/AeroOS Project Report Reviewed.docx. The reviewed layout is ten A4 pages with two-column body text; the teacher explicitly requires at least five pages in two columns. Title and wide figures span columns where appropriate. An ordinary single-column copy would not meet that stated rule.

The title and author details identify the objective, Rahini Sree Duppatla, 25BIT0202, BTech IT, VIT Vellore and the supplied email. The 201-word abstract summarizes the problem, approach, implementation, measured findings and scope. There are six keywords.

I. Introduction states the problem, motivation, objectives and contribution. II. Related Work discusses five genuine papers on model checking, SPIN, symbolic verification, partial-order reduction and concurrency testing. Their methods motivate the project; this implementation is an explicit-state BFS checker, not an implementation of SPIN, symbolic model checking or partial-order reduction. III. Methodology explains architecture, input models, simulation, BFS, shield and bounds. IV. Implementation describes the stack, modules, reproducibility and metrics. V. Results presents measurements, figures, tables and test evidence with interpretation. VI. Conclusion/Future Work summarizes achievements and limitations and identifies extensions. References uses IEEE numbered citations [1]-[5].

Fig. 1 is the internal architecture: React <-> Flask <-> C, with SQLite connected to Flask, and separate modules inside C. It is not a UI screenshot. Fig. 2 graphs waiting-time comparisons on a common CPU workload. Figs. 3 and 4 show the actual desktop and Safety Lab. Four numbered tables show the cyclic model, counterexample, policy metrics and test evidence. Five editable equations explain the model property and timing/throughput metrics. Algorithms are described without source-code listings.

The report was prepared by checking actual code and measured outputs, using genuine cited sources, producing project-specific diagrams/graphs, inserting real UI captures and checking the rendered layout. AI assistance was involved; do not say it was independently authored without assistance. Similarity below 10% and AI-content below 20% have not been measured or guaranteed. Follow the institution's assessment/disclosure rules.

The report's explicit three-job CPU experiment differs from the browser's default 14-job experiment. Different inputs legitimately produce different values. When asked to reproduce a table, use its stated workload and parameters.

## 10. Brief code-file map

| File | What it does |
|---|---|
| start.sh | Enters the correct project folder, builds C, prepares Python if needed, builds absent frontend output and starts Flask. |
| build.sh | Finds Node, installs absent frontend dependencies and checks/builds the TypeScript frontend. |
| core/Makefile | Compiler flags and C build targets. |
| core/include/aero.h | Shared structures, limits, JSON helpers and module interfaces. |
| core/src/main.c | Reads native input and dispatches airport/schedule/banker/deadlock/memory/sync/modelcheck. |
| core/src/json.c | Reads tokenized JSON fields and emits escaped JSON strings. |
| core/src/flight.c | Parses/validates PCB-like records and names states. |
| core/src/scheduler.c | Runs the integer simulation, chooses among nine policies, produces snapshots/Gantt/events and metrics. |
| core/src/resource_manager.c | Checks compatibility/capacity, acquires complete bundles and releases only owned slots. |
| core/src/banker.c | Maximum-claim safety, request rollback, current-request deadlock detection and victim recovery. |
| core/src/memory_manager.c | Contiguous partitions and page frames, policies, traces and translation. |
| core/src/synchronization.c | Creates/joins real pthread workers and implements lock/semaphore experiments. |
| core/src/model_checker.c | Packed progress states, visited map, BFS, safety shield and parent-pointer traces. |
| backend/app.py | Flask routes, validation, fixed subprocess calls, error handling, comparisons and exports. |
| backend/database.py | SQLite schema and scoped persistence operations. |
| backend/scenarios.py | Seeded workloads and editable lab presets, not hardcoded experimental results. |
| frontend/src/main.tsx | Starts React and its error boundary. |
| frontend/src/App.tsx | Bootstraps data, coordinates shared state, navigation, API runs and replay clock. |
| frontend/src/store.tsx | Session context shared across folders. |
| frontend/src/types.ts | TypeScript data records, constants and HTTP helper. |
| frontend/src/Desktop.tsx | Desktop folders and icons. |
| frontend/src/Airport.tsx | Airport map, replay controls, process/resource inspectors and disruptions. |
| frontend/src/Scheduler.tsx | Policy comparisons, objectives and paired experiments. |
| frontend/src/Safety.tsx | Model-checker, Banker and recovery forms/results. |
| frontend/src/Workbench.tsx | Memory and synchronization controls/traces. |
| frontend/src/components.tsx | Shared Gantt, FlightTable, Modal and ResourceGraph components. |
| frontend/src/styles.css | Paper grid, olive desktop, windows and responsive layout styling. |
| frontend/vite.config.ts | React/Tailwind build plugins and development API proxy. |
| frontend/package.json / pnpm-lock.yaml | Frontend scripts/dependencies and resolved versions. |
| tests/test_engine.py | 34 algorithm/integration tests against native outputs. |
| tests/test_verifier.py | Nine verifier tests, including independent oracle comparisons. |
| tests/test_api.py | Six API tests using isolated test storage. |
| tests/test_resources.c | Separate native ownership/capacity assertions. |
| tests/run_all.sh | Builds and runs suites plus sanitizer checks. |
| tests/export_examples.py | Regenerates reproducible example output. |
| data/ | Stored runs/configurations and example results; not source code. |
| docs/ | Architecture, algorithm explanations, test evidence and viva guidance. |
| reports/ | Academic submission report and its requirement checklist. |

If asked to open only a few files, start with frontend/src/Airport.tsx, backend/app.py, core/src/main.c, core/src/scheduler.c, core/src/resource_manager.c and core/src/model_checker.c. Follow one request end to end rather than trying to memorize every line.

## 11. Suggested presentation order

For about eight minutes: introduction/architecture (1 minute), Airport process/resource inspection (1 minute), Scheduler tradeoff (1 minute), Safety Lab counterexample/shield/inconclusive result (2 minutes), memory and threads (2 minutes), report/limitations (1 minute). If time is short, prioritize the safety explorer and mutex-versus-lost-update comparison. Practice explaining why each output occurs, then reproduce it without relying on a memorized script.
