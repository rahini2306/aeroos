import os
import sys
import tempfile
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'backend'))
TEMP=tempfile.TemporaryDirectory()
os.environ['AEROOS_DB']=str(Path(TEMP.name)/'test.sqlite3')
from app import app
from scenarios import scenario,BANKER

class API(unittest.TestCase):
    def setUp(self):self.client=app.test_client()
    def post(self,path,data):
        r=self.client.post('/api'+path,json=data);self.assertLess(r.status_code,300,r.get_json());return r.get_json()
    def test_end_to_end_persistence_reports_replay(self):
        s=scenario();rid=self.post('/simulate',s)['run_id'];r=self.client.get(f'/api/runs/{rid}').get_json();self.assertEqual(r['input'],s)
        saved=self.post('/scenarios',dict(name='Saved test',scenario=s));self.assertEqual(self.client.get('/api/scenarios/'+str(saved['id'])).get_json(),s)
        copy=self.post('/scenarios',dict(name='Copy',scenario=s));self.assertNotEqual(copy['id'],saved['id'])
        snap=self.post('/snapshots',dict(run_id=rid,position=3,name='tick 3'));self.assertIsInstance(snap['id'],int)
        self.assertEqual(r['output']['snapshots'][3],self.client.get(f'/api/runs/{rid}').get_json()['output']['snapshots'][3])
        for f in ['md','json','csv']:
            out=self.client.get(f'/api/reports/{rid}?format={f}');self.assertEqual(out.status_code,200);self.assertIn('attachment',out.headers['Content-Disposition'])
        self.assertEqual(self.client.delete('/api/scenarios/'+str(saved['id'])).status_code,200)
        self.assertEqual(self.client.get('/api/scenarios/'+str(saved['id'])).status_code,404)
    def test_battle_and_experiment(self):
        s=scenario();r=self.post('/battle',dict(scenario=s,algorithms=['fcfs','srtf','rr']));self.assertEqual(len(r['results']),3)
        r=self.post('/experiment',dict(baseline=s,modified=s));self.assertTrue(all(v==0 for v in r['differences'].values()))
    def test_lab_and_bootstrap(self):
        self.assertEqual(self.client.get('/api/bootstrap').status_code,200);self.assertTrue(self.post('/lab/banker',BANKER)['safe'])
        from scenarios import PAGING
        r=self.post('/lab/memory',{**PAGING,'compare':True});self.assertEqual([x['page_faults'] for x in r['comparisons']],[10,9,7])
        self.assertIn(b'page_faults',self.client.get(f"/api/reports/{r['run_id']}?format=csv").data)
        r=self.post('/simulate',dict(flights=[dict(id='MIN',burst=2)]));self.assertEqual(r['scenario']['resources']['runways'],3);self.assertEqual(r['scenario']['flights'][0]['airline'],'AeroOS')
    def test_model_checker_api_and_archived_evidence(self):
        r=self.post('/verify',dict(capacities=[1,1],plans=[[0,1],[1,0]]))
        self.assertEqual(r['baseline']['verdict'],'counterexample')
        self.assertEqual(r['shielded']['verdict'],'verified')
        stored=self.client.get('/api/runs/'+str(r['run_id'])).get_json()
        self.assertEqual(stored['kind'],'modelcheck')
        self.assertEqual(stored['output']['baseline']['states'],r['baseline']['states'])
        self.assertEqual(self.client.post('/api/verify',json=dict(capacities=[1],plans=[[2]])).status_code,400)

    def test_fractional_and_malformed_lab_values_are_rejected(self):
        for kind,payload in [("banker",dict(available=[1.5],maximum=[[1]],allocation=[[0]])),("memory",dict(mode="paging",frames=2,references=[0,1.5])),("sync",dict(mode="mutex",threads=3.5))]:
            self.assertEqual(self.client.post('/api/lab/'+kind,json=payload).status_code,400)

    def test_validation_and_origin(self):
        for bad in [{},dict(flights=[]),{**scenario(),'flights':'not a list'},{**scenario(),'resources':dict(runways=-1)},{**scenario(),'flights':[{**scenario()['flights'][0],'requirements':[1,2]}]}]:self.assertEqual(self.client.post('/api/simulate',json=bad).status_code,400)
        self.assertEqual(self.client.post('/api/snapshots',json=dict(run_id=999999,position=0)).status_code,400)
        self.assertEqual(self.client.post('/api/simulate',json=scenario(),headers={'Origin':'https://untrusted.example'}).status_code,403)
        self.assertEqual(self.client.post('/api/simulate',data='{broken',content_type='application/json').status_code,400)
        self.assertEqual(self.client.post('/api/simulate',json=scenario(),headers={'Host':'attacker.example'}).status_code,403)

if __name__=='__main__':unittest.main(verbosity=2)
