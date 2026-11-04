/* Explicit-state resource model checker.
 * Each process acquires a declared sequence, then atomically releases its bundle.
 * BFS explores every enabled interleaving, merging identical stage vectors.
 * This checks a finite abstract model, NOT arbitrary C code or host pthreads.
 */
#include "aero.h"
#include <stdint.h>
#define MC_N 6
#define MC_R 4
#define MC_STEPS 6

typedef struct {
    unsigned char stage[MC_N];
    int parent, actor, resource, depth;
} ModelState;

typedef struct {
    int n, m, capacity[MC_R], length[MC_N], plan[MC_N][MC_STEPS], limit;
} Model;

static unsigned state_key(const ModelState *s, int n) {
    unsigned key = 0;
    for (int i = 0; i < n; ++i) key |= (unsigned)s->stage[i] << (3 * i);
    return key;
}

static void allocation(const Model *m, const ModelState *s,
                       int owned[MC_N][MC_R], int available[MC_R]) {
    memset(owned, 0, sizeof(int) * MC_N * MC_R);
    memcpy(available, m->capacity, sizeof(int) * MC_R);
    for (int i = 0; i < m->n; ++i) {
        if (s->stage[i] > m->length[i]) continue;
        for (int j = 0; j < s->stage[i]; ++j) {
            int r = m->plan[i][j];
            owned[i][r]++;
            available[r]--;
        }
    }
}

/* Runtime assurance shield: reject a tentative acquisition when maximum-claim
 * reduction cannot establish a completion sequence. The BFS independently
 * checks the transitions retained by this shield. */
static bool safe_grant(const Model *m, const ModelState *s) {
    int owned[MC_N][MC_R], work[MC_R];
    bool finish[MC_N] = {false};
    allocation(m, s, owned, work);
    for (int i = 0; i < m->n; ++i) finish[i] = s->stage[i] > m->length[i];
    for (int pass = 0; pass < m->n; ++pass) {
        bool progress = false;
        for (int i = 0; i < m->n; ++i) {
            if (finish[i]) continue;
            int need[MC_R] = {0};
            for (int j = s->stage[i]; j < m->length[i]; ++j) need[m->plan[i][j]]++;
            bool fit = true;
            for (int r = 0; r < m->m; ++r) if (need[r] > work[r]) fit = false;
            if (!fit) continue;
            for (int r = 0; r < m->m; ++r) work[r] += owned[i][r];
            finish[i] = true;
            progress = true;
        }
        if (!progress) break;
    }
    for (int i = 0; i < m->n; ++i) if (!finish[i]) return false;
    return true;
}

static void print_state(const Model *m, const ModelState *s) {
    int owned[MC_N][MC_R], available[MC_R];
    allocation(m, s, owned, available);
    printf("{\"depth\":%d,\"stages\":[", s->depth);
    for (int i = 0; i < m->n; ++i) printf("%s%d", i ? "," : "", s->stage[i]);
    printf("],\"available\":[");
    for (int r = 0; r < m->m; ++r) printf("%s%d", r ? "," : "", available[r]);
    printf("],\"allocation\":[");
    for (int i = 0; i < m->n; ++i) {
        printf("%s[", i ? "," : "");
        for (int r = 0; r < m->m; ++r) printf("%s%d", r ? "," : "", owned[i][r]);
        putchar(']');
    }
    printf("],\"actor\":%d,\"resource\":%d,\"action\":", s->actor, s->resource);
    quote(s->actor < 0 ? "initial" : s->resource < 0 ? "release" : "acquire");
    printf("}");
}

static void explore(const Model *m, bool shield) {
    ModelState *nodes = calloc((size_t)m->limit, sizeof(*nodes));
    size_t key_count = (size_t)1 << (3 * m->n);
    int *visited = malloc(key_count * sizeof(int));
    if (!nodes || !visited) {free(nodes); free(visited); fail("Model allocation failed");}
    for (size_t i = 0; i < key_count; ++i) visited[i] = -1;
    nodes[0].parent = nodes[0].actor = nodes[0].resource = -1;
    visited[0] = 0;
    int count = 1, edges = 0, pruned = 0, deadlocks = 0, terminal = 0;
    int first_deadlock = -1, first_terminal = -1;
    bool exhaustive = true, capacity_ok = true;
    printf("{\"shield\":%s,\"graph\":{\"edges\":[", shield ? "true" : "false");
    int drawn_edges = 0;
    for (int head = 0; head < count; ++head) {
        ModelState current = nodes[head];
        int owned[MC_N][MC_R], available[MC_R];
        allocation(m, &current, owned, available);
        for (int r = 0; r < m->m; ++r) if (available[r] < 0) capacity_ok = false;
        bool done = true, enabled = false;
        for (int i = 0; i < m->n; ++i) if (current.stage[i] <= m->length[i]) done = false;
        if (done) {terminal++; if (first_terminal < 0) first_terminal = head; continue;}
        for (int i = 0; i < m->n; ++i) {
            if (current.stage[i] > m->length[i]) continue;
            int resource = current.stage[i] == m->length[i] ? -1 : m->plan[i][current.stage[i]];
            if (resource >= 0 && available[resource] == 0) continue;
            ModelState next = current;
            next.stage[i]++;
            next.actor = i; next.resource = resource;
            next.parent = head; next.depth = current.depth + 1;
            if (shield && resource >= 0 && !safe_grant(m, &next)) {pruned++; continue;}
            enabled = true; edges++;
            unsigned key = state_key(&next, m->n);
            int index = visited[key];
            if (index < 0) {
                if (count >= m->limit) {exhaustive = false; continue;}
                index = count; visited[key] = count; nodes[count++] = next;
            }
            if (head < 160 && index < 160 && drawn_edges < 700) {
                printf("%s[%d,%d,%d]", drawn_edges ? "," : "", head, index, i);
                drawn_edges++;
            }
        }
        if (!enabled) {deadlocks++; if (first_deadlock < 0) first_deadlock = head;}
    }
    printf("],\"nodes\":[");
    for (int i = 0; i < count && i < 160; ++i) {
        if (i) putchar(',');
        print_state(m, nodes + i);
    }
    printf("],\"truncated\":%s},\"states\":%d,\"transitions\":%d,\"shield_rejections\":%d,"
           "\"deadlocks\":%d,\"terminal_states\":%d,\"exhaustive\":%s,\"capacity_invariant\":%s,\"verdict\":",
           count > 160 || edges > drawn_edges ? "true" : "false", count, edges, pruned,
           deadlocks, terminal, exhaustive ? "true" : "false", capacity_ok ? "true" : "false");
    quote(deadlocks ? "counterexample" : exhaustive ? "verified" : "inconclusive");
    int target = first_deadlock >= 0 ? first_deadlock : first_terminal;
    printf(",\"trace_kind\":");
    quote(first_deadlock >= 0 ? "shortest_deadlock" : first_terminal >= 0 ? "completion_witness" : "none");
    printf(",\"trace\":[");
    if (target >= 0) {
        int order[MC_N * (MC_STEPS + 1) + 1], n = 0;
        for (int k = target; k >= 0; k = nodes[k].parent) order[n++] = k;
        for (int k = n - 1; k >= 0; --k) {if (k != n - 1) putchar(',');print_state(m, nodes + order[k]);}
    }
    printf("]}");
    free(nodes); free(visited);
}

void model_check(int root) {
    Model m = {0};
    int caps = field(root, "capacities"), plans = field(root, "plans");
    m.n = tok[plans].size; m.m = tok[caps].size;
    m.limit = get(root, "max_states", 20000);
    if (m.n < 1 || m.n > MC_N || m.m < 1 || m.m > MC_R || m.limit < 1 || m.limit > 50000)
        fail("Model bounds: 1..6 processes, 1..4 resources, 1..50000 states");
    for (int r = 0; r < m.m; ++r) {
        m.capacity[r] = number(item(caps, r), -1);
        if (m.capacity[r] < 1 || m.capacity[r] > 6) fail("Capacity must be 1..6");
    }
    for (int i = 0; i < m.n; ++i) {
        int row = item(plans, i); m.length[i] = tok[row].size;
        if (m.length[i] < 1 || m.length[i] > MC_STEPS) fail("Each plan needs 1..6 acquisitions");
        int claim[MC_R] = {0};
        for (int j = 0; j < m.length[i]; ++j) {
            int r = number(item(row, j), -1);
            if (r < 0 || r >= m.m) fail("Plan refers to unknown resource");
            m.plan[i][j] = r;
            if (++claim[r] > m.capacity[r]) fail("A process claim exceeds total capacity");
        }
    }
    printf("{\"model\":\"finite-resource-interleavings\",\"property\":\"AG(completed OR enabled_action)\","
           "\"scope\":\"Declared acquisition plans only; not host threads or the airport service engine\",\"baseline\":");
    explore(&m, false);
    printf(",\"shielded\":");
    explore(&m, true);
    puts("}");
}
