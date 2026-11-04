"""Independent transition-system oracle and native verifier regression tests."""
import collections
import json
import os
import random
import subprocess
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def native(model):
    binary=os.environ.get('AEROOS_TEST_ENGINE',str(ROOT/'core/aeroos'))
    p=subprocess.run([binary,'modelcheck'],input=json.dumps(model),text=True,capture_output=True,timeout=20)
    if p.returncode: raise ValueError(p.stdout)
    return json.loads(p.stdout)

def oracle(caps,plans):
    start=tuple(0 for _ in plans)
    queue=collections.deque([start]);seen={start};edges=0;dead=[]
    while queue:
        state=queue.popleft();free=caps.copy()
        for i,plan in enumerate(plans):
            if state[i]<=len(plan):
                for resource in plan[:state[i]]:free[resource]-=1
        if all(x>len(plan) for x,plan in zip(state,plans)):continue
        successors=[]
        for i,plan in enumerate(plans):
            if state[i]>len(plan):continue
            if state[i]<len(plan) and free[plan[state[i]]]<=0:continue
            child=list(state);child[i]+=1;child=tuple(child);successors.append(child);edges+=1
            if child not in seen:seen.add(child);queue.append(child)
        if not successors:dead.append(state)
    return len(seen),edges,len(dead)

class ModelChecker(unittest.TestCase):
    def test_three_process_shortest_counterexample_and_shield(self):
        r=native(dict(capacities=[1,1,1],plans=[[0,1],[1,2],[2,0]]))
        self.assertEqual(r['baseline']['states'],45)
        self.assertEqual(r['baseline']['deadlocks'],1)
        self.assertEqual(len(r['baseline']['trace'])-1,3)
        self.assertEqual(r['shielded']['verdict'],'verified')
        self.assertTrue(r['shielded']['exhaustive'])
        self.assertEqual(r['shielded']['shield_rejections'],3)
    def test_two_process_trace_is_executable(self):
        model=dict(capacities=[1,1],plans=[[0,1],[1,0]])
        r=native(model)
        trace=r['baseline']['trace'];self.assertEqual(len(trace),3)
        for before,after in zip(trace,trace[1:]):
            actor=after['actor'];resource=after['resource']
            self.assertGreater(before['available'][resource],0)
            self.assertEqual(after['stages'][actor],before['stages'][actor]+1)
        self.assertEqual(trace[-1]['available'],[0,0])
        self.assertEqual(trace[-1]['stages'],[1,1])
    def test_noncyclic_order_is_verified(self):
        r=native(dict(capacities=[1,1],plans=[[0,1],[0,1],[0,1]]))
        self.assertEqual(r['baseline']['verdict'],'verified')
        self.assertEqual(r['baseline']['deadlocks'],0)
    def test_more_instances_remove_deadlock(self):
        r=native(dict(capacities=[2,2],plans=[[0,1],[1,0]]))
        self.assertEqual(r['baseline']['verdict'],'verified')
    def test_limit_is_never_a_safety_proof(self):
        r=native(dict(capacities=[1,1],plans=[[0,1],[1,0]],max_states=1))
        for key in ['baseline','shielded']:
            self.assertFalse(r[key]['exhaustive']);self.assertEqual(r[key]['verdict'],'inconclusive')
    def test_random_models_match_independent_oracle(self):
        rng=random.Random(27)
        for _ in range(35):
            caps=[rng.randint(1,2) for _ in range(2)]
            plans=[]
            for _ in range(rng.randint(1,4)):
                plans.append(rng.sample(range(2),rng.randint(1,2)))
            r=native(dict(capacities=caps,plans=plans))
            expected=oracle(caps,plans);b=r['baseline']
            self.assertEqual((b['states'],b['transitions'],b['deadlocks']),expected)
            self.assertEqual(r['shielded']['deadlocks'],0)
            self.assertTrue(r['shielded']['capacity_invariant'])
            self.assertTrue(r['shielded']['exhaustive'])
    def test_repeatability(self):
        m=dict(capacities=[1,1],plans=[[0,1],[1,0]])
        self.assertEqual(native(m),native(m))
    def test_completed_trace_releases_every_resource(self):
        m=dict(capacities=[2,1],plans=[[0,0,1],[1,0]])
        r=native(m);state=r['shielded']['trace'][-1]
        self.assertEqual(state['available'],m['capacities'])
        self.assertTrue(all(all(x==0 for x in row) for row in state['allocation']))
    def test_invalid_claims_and_bounds(self):
        for model in [dict(capacities=[1],plans=[[0,0]]),dict(capacities=[1],plans=[[1]]),dict(capacities=[],plans=[]),dict(capacities=[1],plans=[[0]],max_states=0)]:
            with self.assertRaises(ValueError):native(model)

if __name__=='__main__':unittest.main(verbosity=2)
