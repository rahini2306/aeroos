# Beyond the syllabus: counterexample-driven OS verification

## The academic contribution

The provided syllabus already covers classical scheduling, IPC, semaphores, deadlock, memory policies, RTOS and ROS. AeroOS keeps those as supporting experiments. Its distinguishing feature now asks a different question: **not “is this state deadlocked?”, but “can any reachable allocation interleaving deadlock?”**

This uses explicit-state model checking, a formal-methods technique not listed in that syllabus. It is an established technique, not a newly invented algorithm or a guarantee of approval by a reviewer. The implementation is independent C code, not a SPIN/CBMC wrapper.

## What is actually implemented

- A finite transition system in `core/src/model_checker.c`.
- Breadth-first search over process-stage vectors, with direct-index state deduplication.
- Resource-capacity invariants over every explored state.
- Reachable nonterminal dead-end detection.
- Parent chains producing a shortest deadlock witness.
- Maximum-claim safety checks as an online grant shield.
- Independent exhaustive exploration of the shielded transition system.
- A graph preview, a stepwise trace inspector, recorded evidence exports and API persistence.
- A hard state limit that produces `inconclusive`, never a false proof.

## Model contract

Each of 1–6 service processes declares 1–6 resource acquisitions in a fixed order. There are 1–4 resource classes, with 1–6 interchangeable instances per class. A process holds each acquired instance until its whole acquisition plan is fulfilled, then one atomic release action returns all its instances and marks it complete. Every enabled process can take the next step. States are tuples of process stages; availability/allocation are derived exactly from those stages. Acquisitions cannot exceed available capacity. A single process's full claim cannot exceed total capacity. This model has no arrivals, timed outages, host locks, priorities, retries, failures, physical runways or unbounded process creation.

The checked property is `AG(complete OR enabled_action)`: every reachable state is terminal or has a legal successor. It is a deadlock-freedom property over this finite model, not general temporal-logic parsing. Because stages only increase, there are no cycles or infinite non-stuttering executions within the declared plans. Host scheduling starvation is outside the model.

The state budget is an exploration resource bound, not a SAT unrolling-depth bound. Do not describe this as SAT-based bounded model checking, LTL synthesis, CEGAR, or verification of arbitrary C code.

## Safety shield

For a tentative acquisition, construct Allocation, Available and the remaining maximum claim. Run Work/Finish safety reduction. Reject the grant if no guaranteed completion order remains. This is a classical Banker-style safety predicate; the novelty claim is **not** Banker itself. The model checker then independently explores all retained transitions to check deadlock freedom. The shield can conservatively reject some schedules that would succeed with a cooperative future; comparison results count actual rejected transitions.

## Reproducible demonstration

Capacities `[1,1,1]`; acquisition plans P0 `[0,1]`, P1 `[1,2]`, P2 `[2,0]`.

The unprotected state graph has 45 states and one deadlock. BFS returns three acquisitions: each process holds its first resource and requests the next. With the shield, the reachable graph has 44 states, zero deadlocks, and three rejected unsafe transitions. The shielded completion trace is just one witness; the exhaustive graph result is what supports the no-deadlock claim.

Try resource capacities `[2,2]` with plans `[0,1]` and `[1,0]`: a dependency cycle does not cause a deadlock when sufficient instances exist. Try a state limit of 1: the result must say inconclusive. These make useful viva counterexamples.

## Tests and scientific honesty

Nine verifier test methods include exact examples, executable trace validation, completion/resource-release checks, extra-instance cases, invalid claims, limit semantics and repeatability. The baseline explored-state/transition/deadlock counts are compared with an independent Python oracle on 35 seeded models. C is the product implementation; Python is a test oracle only. ASan/UBSan runs all nine verifier tests. Tests are evidence for the implementation, not a proof that the checker itself has no bugs.

## Five-minute review narrative

1. State the system model and draw the three resource plans.
2. Explain why one successful simulation schedule proves little about other interleavings.
3. Run unprotected exploration and replay the shortest counterexample.
4. Inspect held/available resources at the dead-end state.
5. Switch to the shielded graph and explain what maximum-claim checking rejects.
6. Show exhaustive exploration, rather than just the successful witness.
7. Reduce the state budget to demonstrate an inconclusive result.
8. Explain limits: this verifies the small abstract model, not physical airport safety or arbitrary pthread execution.

## Primary references

- [CMU model-checking guided tour](https://www.cs.cmu.edu/~modelcheck/tour.htm): transition systems and counterexamples.
- [SPIN manual](https://spinroot.com/spin/Man/Manual.html): explicit-state model exploration and verification practice.
- [CMU SEI model-based verification analysis guidelines](https://www.sei.cmu.edu/library/model-based-verification-analysis-guidelines/): interpreting finite model-checking results and analysis limits.

These sources motivate the standard technique. AeroOS does not use their names to imply certification or claim an invented formal method.
