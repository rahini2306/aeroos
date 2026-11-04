# Implemented algorithms and protocol

## Scheduling

All policies share `scheduler(root, airport)` and the same process records, boundary clock, Gantt output, and metric formulas. Emergencies precede regular work at dispatch. Ties use arrival then flight ID, except FIFO queue stamps in RR/MLFQ.

| Policy | Implemented rule |
|---|---|
| FCFS | Earliest arrival among eligible processes; non-preemptive |
| SJF | Smallest full burst; non-preemptive |
| SRTF | Smallest remaining burst; preemptive abstract CPU |
| Priority | Lowest effective priority; non-preemptive |
| Preemptive priority | Reevaluate effective priority each boundary |
| RR | FIFO enqueue order, configurable quantum; arrivals at the expiry boundary precede the expired task |
| HRRN | Largest `(waiting + burst) / burst`; non-preemptive |
| MLQ | Static priority bands `floor(base_priority/3)`; FCFS within a band; higher bands preempt only in abstract context |
| MLFQ | Three queues; new tasks enter level 0; per-level allotments q, 2q, 4q; demote on exhaustion; FIFO within level; retain used allotment when interrupted by higher queue |

Airport mode accepts FCFS, SJF, priority, HRRN, and non-preemptive MLQ only. Non-preemptibility takes precedence over newly arrived emergencies. Aging applies to priority values: `effective = max(0, base − floor(ready_wait / aging_interval))`; resource-blocked wait does not age the process. Aging does not change FCFS/SJF ordering or periodically boost MLFQ.

Scanning candidates is O(n) per dispatch, with up to n dispatch attempts per boundary in airport mode. Resource scans make a bound O(T n² R S); these small educational workloads favor transparent deterministic behavior over asymptotically optimized heaps. HRRN evaluates ratios at dispatch. RR and MLFQ do not depend on host thread scheduling.

## Timing and fairness

- Waiting: sum of every READY and BLOCKED unit interval, including preemption intervals.
- Resource waiting: BLOCKED intervals only.
- Turnaround: completion − arrival, for completed jobs.
- Response: first dispatch − arrival; −1 if never dispatched.
- Average waiting: all configured jobs, including currently unfinishable/cancelled jobs.
- Average turnaround: completed jobs only; average response: dispatched jobs only. Interpret partial-run populations carefully.
- Throughput: completed jobs / elapsed ticks; zero at elapsed time zero.
- Maximum wait: largest accumulated wait.
- Service utilization: executed service ticks / (elapsed ticks × configured runway count), or elapsed ticks on one abstract CPU.
- Context switches: changes from one process to another on the abstract CPU; initial dispatch and idle transitions are excluded. Airport context reports zero CPU context switches.
- Starvation indicators: ready wait >= threshold. These are warnings over a finite workload, not proof of infinite starvation. Resource shortages are counted separately.
- Queue averages integrate READY+BLOCKED process count over unit intervals. Failed requests count each failed admission attempt; contention ticks are per-resource/per-process unmet requests per boundary.

## Resources and emergency faults

Each flight requires exactly one runway in airport mode. Six-class resource requirements can additionally request gates, baggage, maintenance, fuel, and crew. The model holds all requested slots and memory for the burst. Runway/gate slot classes may be `all`, `light`, `medium`, or `heavy`. All-or-nothing compatible acquisition prevents hold-and-wait. Acquiring an already allocated process is rejected; releasing process i only clears slots actually owned by i. Completion/cancellation release memory and resources.

Faults reduce the admission capacity of a resource for `[time,time+duration)`. A closure never revokes an existing owner, including owners in slots above the reduced capacity. Capacity changes trigger normal dispatch reevaluation and are logged. Multiple reductions add and clamp at zero. Reported utilization keeps configured capacity in the denominator; it does not conceal outages by shrinking the denominator.

## Banker and deadlock

`Need = Maximum − Allocation`. Request components must be nonnegative, <= Need, and <= Available. Tentative grants mutate Available, Allocation, and Need. Safety reduction repeatedly selects an unfinished process whose Need fits Work, records the comparison/release, then adds its Allocation to Work. If no complete sequence exists, safe mode restores all matrices; normal mode retains a valid available grant while reporting unsafe. No request means analysis only.

Deadlock detection uses **outstanding Request**, not maximum Need. Initialize processes with no owned resources as finished; repeatedly simulate completions whose outstanding requests fit Work. Unfinished resource-holding processes are deadlocked. Wait-for dependencies are derived separately; cycle existence is never substituted for multi-instance reduction. A single-instance deadlock is also handled by this general reduction. The graph's necessary conditions are model/graph observations, not independent proofs for multiple instances.

Recovery terminates an explicit victim or the affected process with the fewest allocated units. It releases that process's Allocation, clears its requests, and reruns reduction. `restored=false` means one victim was insufficient; the app does not invent recovery. Before/after allocation/request matrices and remaining affected indices are returned. The airport model prevents deadlock with atomic bundles; the concurrency dining lab prevents circular wait with a total fork order. Banker avoidance is an independent editable lab rather than an airport admission policy.

Safety/detection complexity: O(n²m), storage O(nm). Graph cycle inspection uses DFS over the process dependency adjacency matrix.

## Memory

Contiguous: First Fit selects earliest adequate hole; Best/Worst choose smallest/largest adequate hole. Next Fit starts at the last allocation cursor, including the interior of a coalesced hole, and wraps. Exact-size variable partitions produce zero internal fragmentation. Free and swap-out release process-owned blocks and coalesce adjacent holes. Compaction packs live blocks in address order. External fragmentation = free total − largest free hole. Failed and duplicate process allocations do not mutate ownership.

Paging: references are virtual page numbers of one abstract address space. A miss demand-loads a page into an empty frame or evicts by FIFO insertion time, LRU last access, or Optimal farthest future access. Optimal is an offline theoretical benchmark. A page fault means non-residency even if an empty frame exists. A replacement is an eviction, so initial fills are faults but not replacements. `physical = frame_index * page_size + offset`; −1 means non-resident. Optional `process_bytes` demonstrates final-page internal fragmentation; absent byte size assumes full pages. Fault rate >70% is a documented educational thrashing warning, not a measured OS working-set classifier.

Contiguous allocation scans holes O(b); compaction O(b). FIFO/LRU lookup/replacement O(f) per reference. Optimal scans future references O(k²f) in this implementation.

## POSIX laboratory

Mutex and binary semaphore serialize critical sections. Counting permits allow a configurable number of concurrent critical entries; a separate audit mutex makes counter updates and traces safe. Producer/consumer uses a bounded 16-item buffer and empty/full semaphores, with paired producers/consumers. Readers acquire shared rwlocks, writers exclusive rwlocks. Dining threads acquire the lower-numbered fork before the higher-numbered fork. Race mode uses atomics and a condition-variable barrier so multiple threads load the same old value before storing an increment: a repeatable lost-update lesson without undefined C data races. Named semaphores are unlinked immediately after creation and closed after all threads join, supporting macOS where unnamed `sem_init` is unavailable.

## Snapshot tuple protocol

`processes[i]` = `[state, remaining, waiting, first_dispatch, completion, effective_priority, bypass_count, resource_wait, runway_count, gate_count, baggage_count, maintenance_count, fuel_count, crew_count]`.

State indices: NEW=0, READY=1, RUNNING=2, WAITING=3, BLOCKED=4, COMPLETED=5, CANCELLED=6. The implemented airport uses BLOCKED for denied bundles; WAITING is reserved for future phased I/O. Valid used transitions: NEW→READY/CANCELLED, READY→RUNNING/BLOCKED/CANCELLED, BLOCKED→RUNNING/CANCELLED, RUNNING→READY (abstract preemption), RUNNING→COMPLETED/CANCELLED. Terminal states never reenter service. Arrival can produce NEW→READY→RUNNING at one boundary and records both transitions.

`owners[r][slot]` is a zero-based process index or −1. Capacity arrays define current admission limits. Events have deterministic sequential IDs, timestamps, types, and nest under their snapshot, with from/to/reason plus dispatch algorithm, actual eligible/deferred indices, free-slot counts, and available memory captured at the decision boundary. Gantt slices store process/start/end/lane. IDs in frontend event lists combine snapshot time and event ordinal. Replay stores complete snapshots; it never reruns pthread scheduling.

Fairness index uses Jain's index over completed jobs' service fractions `x_i = burst_i / turnaround_i`: `(sum x)^2 / (n * sum x²)`. It is 0 with no completed jobs. A value near 1 means similar normalized service fractions, not necessarily short waits or equal priorities. Emergency mean response averages dispatched emergency jobs only; zero also covers a workload with no emergencies and should be interpreted with the workload.
