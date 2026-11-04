# Test coverage

Run `./tests/run_all.sh --sanitize` from the project root. Tests use temporary SQLite databases rather than deleting saved application records. The native resource assertion harness is built in a temporary location.

| Suite | Coverage |
|---|---|
| Native resource assertions | Capacity, exclusive ownership, memory admission, compatibility and owner-only release. |
| Engine tests | Nine scheduling policies, deterministic replay, cancellations, closures, resource ownership, Banker safety, actual deadlock recovery, memory policies and seven real-thread modes. |
| Verifier tests | Shortest traces, release after completion, repeated units, invalid claims, repeatability, state-limit semantics and 35 seeded baseline comparisons with an independent oracle. |
| API tests | Validated requests, persistence, scenario operations, replay positions, exports, comparisons, model-checker evidence and rejection of malformed input/origins. |
| Frontend | Strict TypeScript checking and production Vite build. |

There are 49 automated test methods: 34 engine, nine verifier and six API tests. The direct C harness is separate. The sanitizer command repeats the 34 engine and nine verifier methods with AddressSanitizer and UndefinedBehaviorSanitizer. The 35 seeded oracle models are subcases within one verifier method, not 35 extra test methods.

Current cleanup verification is saved in `test-results-2026-10-29.txt`. Earlier dated evidence is retained for the original audit. Focused browser checks are described in `browser-checks.md`; they are not an exhaustive device or accessibility matrix. No ThreadSanitizer or exhaustive long-duration leak audit has been performed.
