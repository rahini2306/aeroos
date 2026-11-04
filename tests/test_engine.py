"""Black-box tests call the independently executable C engine, never a Python scheduler."""
import json
import os
import random
import subprocess
import sys
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'backend'))
from scenarios import BANKER,DEADLOCK,PAGING,CONTIGUOUS,scenario,NAMES

def engine(command,data):
    p=subprocess.run([os.environ.get('AEROOS_TEST_ENGINE',str(ROOT/'core/aeroos')),command],input=json.dumps(data),capture_output=True,text=True,timeout=20)
    if p.returncode: raise ValueError(p.stdout)
    return json.loads(p.stdout)

def workload(algorithm='fcfs'):
    return dict(algorithm=algorithm,flights=[dict(id='P0',arrival=0,burst=8,priority=3),dict(id='P1',arrival=1,burst=4,priority=1),dict(id='P2',arrival=2,burst=2,priority=2)],quantum=2)

class Scheduling(unittest.TestCase):
    def test_fcfs_known_metrics(self):
        r=engine('schedule',workload());self.assertEqual([f['waiting'] for f in r['flights']],[0,7,10]);self.assertAlmostEqual(r['metrics']['average_waiting'],17/3,places=4)
        self.assertEqual(r['gantt'],[dict(process=0,start=0,end=8,lane=0),dict(process=1,start=8,end=12,lane=0),dict(process=2,start=12,end=14,lane=0)])
    def test_sjf(self):
        r=engine('schedule',workload('sjf'));self.assertEqual([x['process'] for x in r['gantt']],[0,2,1])
    def test_srtf(self):
        r=engine('schedule',workload('srtf'));self.assertEqual([f['completion'] for f in r['flights']],[14,7,4]);self.assertEqual([f['waiting'] for f in r['flights']],[6,2,0])
    def test_priority(self):
        self.assertEqual([x['process'] for x in engine('schedule',workload('priority'))['gantt']],[0,1,2])
        self.assertEqual([f['completion'] for f in engine('schedule',workload('priority_preemptive'))['flights']],[14,5,7])
    def test_round_robin(self):
        r=engine('schedule',workload('rr'));self.assertEqual([x['process'] for x in r['gantt']],[0,1,2,0,1,0]);self.assertEqual([f['completion'] for f in r['flights']],[14,10,6])
    def test_hrrn(self):
        self.assertEqual([x['process'] for x in engine('schedule',workload('hrrn'))['gantt']],[0,2,1])
    def test_mlq_and_mlfq(self):
        for alg in ['mlq','mlfq']:
            r=engine('schedule',workload(alg));self.assertEqual(r['metrics']['completed'],3)
            self.assertGreater(len(r['gantt']),1)
    def test_mlfq_feedback_and_fifo(self):
        w=dict(algorithm='mlfq',quantum=1,flights=[dict(id='P0',arrival=0,burst=5),dict(id='P1',arrival=0,burst=5)])
        r=engine('schedule',w);self.assertEqual([x['process'] for x in r['gantt']],[0,1,0,1,0,1]);self.assertEqual([x['end']-x['start'] for x in r['gantt']],[1,1,2,2,2,2])
    def test_exact_transition_replay(self):
        r=engine('schedule',workload('rr'))
        self.assertTrue(any(e.get('from')=='RUNNING' and e['to']=='READY' for s in r['snapshots'] for e in s['events']))
        for i,f in enumerate(r['flights']):self.assertEqual(r['snapshots'][-1]['processes'][i][2],f['waiting'])
    def test_idle(self):
        w=dict(flights=[dict(id='late',arrival=5,burst=2)])
        r=engine('schedule',w);self.assertEqual(r['gantt'][0]['start'],5);self.assertEqual(r['metrics']['duration'],7)
        unicode_flight=dict(id='航班✈',airline='भारत Aviation',arrival=0,burst=1);r=engine('schedule',dict(flights=[unicode_flight]));self.assertEqual(r['flights'][0]['id'],unicode_flight['id']);self.assertEqual(r['flights'][0]['airline'],unicode_flight['airline'])
    def test_randomized_invariants_and_determinism(self):
        rng=random.Random(81)
        for alg in ['fcfs','sjf','srtf','priority','priority_preemptive','rr','hrrn','mlq','mlfq']:
            w=dict(algorithm=alg,aging=True,flights=[dict(id=f'P{i}',arrival=rng.randrange(9),burst=rng.randrange(1,9),priority=rng.randrange(9)) for i in range(12)])
            r=engine('schedule',w);self.assertEqual(r,engine('schedule',w));self.assertEqual(r['metrics']['completed'],12)
            for i,f in enumerate(r['flights']):
                self.assertEqual(f['turnaround'],f['waiting']+f['burst']);self.assertGreaterEqual(f['response'],0)
                self.assertEqual(sum(x['end']-x['start'] for x in r['gantt'] if x['process']==i),f['burst'])
            for s in r['snapshots']: self.assertLessEqual(sum(p[0]==2 for p in s['processes']),1)
    def test_aging_changes_priority(self):
        w=workload('priority');w['aging']=True;w['aging_interval']=1;r=engine('schedule',w)
        self.assertLess(r['snapshots'][7]['processes'][1][5],1)

class Resources(unittest.TestCase):
    def test_ownership_capacity_and_release(self):
        w=scenario('Chaos Mode');r=engine('airport',w)
        self.assertFalse(r['metrics']['incomplete'])
        for s in r['snapshots']:
            for resource,owners in enumerate(s['owners']):
                for i,o in enumerate(owners):
                    if o>=0:self.assertEqual(s['processes'][o][0],2)
                for i,p in enumerate(s['processes']):self.assertEqual(p[8+resource],owners.count(i))
            self.assertLessEqual(s['memory_used'],w['resources']['memory'])
        self.assertTrue(all(all(o<0 for o in row) for row in r['snapshots'][-1]['owners']))
    def test_incompatible_and_insufficient(self):
        w=scenario();w['resources']['runway_classes']=['light']*3;r=engine('airport',w)
        self.assertTrue(r['metrics']['incomplete']);self.assertEqual(r['metrics']['completed'],0)
        w=scenario();w['resources']['gates']=0;r=engine('airport',w);self.assertEqual(r['metrics']['completed'],0)
    def test_closure_preserves_owner(self):
        w=dict(flights=[dict(id='A',arrival=0,burst=5),dict(id='E',arrival=1,burst=1,emergency=True)],resources=dict(runways=1,gates=1),faults=[dict(time=1,duration=6,resource=0,reduction=1)])
        r=engine('airport',w);self.assertEqual(r['flights'][0]['completion'],5);self.assertEqual(r['gantt'][1]['start'],7)
    def test_cancellation_releases_only_owned_resources(self):
        w=dict(flights=[dict(id='A',arrival=0,burst=8,cancel_at=2),dict(id='B',arrival=0,burst=3)],resources=dict(runways=1,gates=1))
        r=engine('airport',w);self.assertEqual(r['metrics']['cancelled'],1);self.assertEqual(r['flights'][0]['state'],'CANCELLED');self.assertEqual(r['gantt'][1]['start'],2);self.assertEqual(r['flights'][1]['completion'],5)
    def test_fault_events_and_actual_decision_alternatives(self):
        w=scenario('Chaos Mode');r=engine('airport',w)
        self.assertTrue(any(e['to']=='RESOURCE_CHANGE' for s in r['snapshots'] for e in s['events']))
        self.assertTrue(any('resource_availability' in e for s in r['snapshots'] for e in s['events']))
    def test_nonpreemptive_context(self):
        for a in ['rr','srtf','priority_preemptive','mlfq']:
            with self.assertRaises(ValueError):engine('airport',workload(a))
    def test_all_builtin_replay(self):
        for name in NAMES:
            w=scenario(name);r=engine('schedule' if w['context']=='tasks' else 'airport',w)
            self.assertEqual(r['snapshots'][0]['time'],0);self.assertEqual(r,engine('schedule' if w['context']=='tasks' else 'airport',w))

class BankerDeadlock(unittest.TestCase):
    def test_safe_sequence(self):
        r=engine('banker',BANKER);self.assertTrue(r['safe']);self.assertEqual(len(r['sequence']),5)
        self.assertEqual(r['steps'][0]['work_before'],BANKER['available'])
    def test_safe_grant(self):
        r=engine('banker',{**BANKER,'request_process':1,'request_vector':[1,0,2]});self.assertTrue(r['granted']);self.assertTrue(r['safe']);self.assertEqual(r['available'],[2,3,0])
    def test_rollback_and_normal_mode(self):
        w={**BANKER,'request_process':4,'request_vector':[3,3,0]}
        r=engine('banker',w);self.assertFalse(r['granted']);self.assertEqual(r['allocation'],BANKER['allocation']);self.assertEqual(r['available'],BANKER['available'])
        r=engine('banker',{**w,'safe_mode':False});self.assertTrue(r['granted']);self.assertFalse(r['safe']);self.assertFalse(r['deadlocked'])
    def test_invalid_request(self):
        r=engine('banker',{**BANKER,'request_process':1,'request_vector':[20,0,0]});self.assertFalse(r['granted'])
        with self.assertRaises(ValueError):engine('banker',{**BANKER,'maximum':[[0]]})
    def test_deadlock_and_recovery(self):
        r=engine('deadlock',{**DEADLOCK,'recover':True});self.assertTrue(r['deadlocked']);self.assertEqual(r['affected'],[0,1]);self.assertTrue(r['recovery']['restored'])
    def test_multiinstance_cycle_not_deadlock(self):
        r=engine('deadlock',{**DEADLOCK,'available':[1,0]});self.assertFalse(r['deadlocked'])
        r=engine('deadlock',dict(available=[0,0],allocation=[[1,0],[0,1],[1,0]],request=[[0,1],[1,0],[0,0]]));self.assertTrue(r['conditions']['circular_wait']);self.assertFalse(r['deadlocked'])

class Memory(unittest.TestCase):
    def test_replacement_faults(self):
        for alg,faults in [('fifo',10),('lru',9),('optimal',7)]:self.assertEqual(engine('memory',{**PAGING,'algorithm':alg})['page_faults'],faults)
    def test_translation(self):
        r=engine('memory',dict(mode='paging',algorithm='lru',frames=2,references=[2,4],page_size=256,logical_address=2*256+17));self.assertEqual(r['translation']['physical'],17)
        self.assertEqual(engine('memory',{**PAGING,'logical_address':99999})['translation']['physical'],-1)
    def test_internal_fragmentation(self):
        r=engine('memory',{**PAGING,'process_bytes':1000,'page_size':256,'references':[0,1,2,3]});self.assertEqual(r['internal_fragmentation'],24)
    def test_next_fit_cursor_inside_coalesced_hole(self):
        r=engine('memory',dict(mode='contiguous',algorithm='next',size=100,operations=[dict(process=0,size=20),dict(process=1,size=20),dict(action='free',process=1),dict(process=2,size=10)]));self.assertEqual(next(b['start'] for b in r['steps'][-1]['blocks'] if b['owner']==2),40)
    def test_contiguous_policies(self):
        for alg in ['first','best','worst','next']:
            r=engine('memory',{**CONTIGUOUS,'algorithm':alg});self.assertEqual(r['failed_allocations'],0);self.assertEqual(r['free'],594)
            for s in r['steps']:
                self.assertEqual(sum(b['size'] for b in s['blocks']),1024)
                self.assertTrue(all(a['start']+a['size']==b['start'] for a,b in zip(s['blocks'],s['blocks'][1:])))
    def test_compaction_swap_fragmentation(self):
        r=engine('memory',{**CONTIGUOUS,'operations':CONTIGUOUS['operations']+[dict(action='compact'),dict(action='swap',process=0)]});self.assertEqual(r['free'],774);self.assertTrue(all(s['success'] for s in r['steps']))
    def test_duplicate_owner_and_oversized(self):
        r=engine('memory',dict(mode='contiguous',operations=[dict(process=0,size=20),dict(process=0,size=20)]));self.assertEqual(r['failed_allocations'],1)

class Synchronization(unittest.TestCase):
    def test_real_thread_labs(self):
        for mode in ['mutex','binary','counting','race','producer_consumer','readers_writers','dining']:
            r=engine('sync',dict(mode=mode,threads=4,iterations=30,capacity=2));self.assertEqual(r['joined'],4);self.assertEqual(sum(e['action']=='terminated' for e in r['events']),4)
            if mode in ['mutex','binary','counting','dining']:self.assertEqual(r['counter'],120)
            if mode in ['mutex','binary']:self.assertLessEqual(r['peak_concurrent'],1)
            if mode=='counting':self.assertLessEqual(r['peak_concurrent'],2)
            if mode=='race':self.assertEqual(r['counter'],30)
            if mode=='producer_consumer':self.assertEqual(r['produced'],60);self.assertEqual(r['consumed'],60)
            if mode=='readers_writers':self.assertEqual(r['counter'],60)
    def test_validation(self):
        with self.assertRaises(ValueError):engine('sync',dict(mode='producer_consumer',threads=3))

if __name__=='__main__':unittest.main(verbosity=2)
