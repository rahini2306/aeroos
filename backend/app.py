import copy
import csv
import io
import json
import math
import subprocess
from pathlib import Path
from urllib.parse import urlparse
from flask import Flask, jsonify, request, send_from_directory, Response
from werkzeug.exceptions import HTTPException
import database
from scenarios import scenario, NAMES, BANKER, DEADLOCK, PAGING, CONTIGUOUS

ROOT = Path(__file__).resolve().parents[1]
app = Flask(__name__, static_folder=None)
app.config["MAX_CONTENT_LENGTH"] = 512 * 1024
database.init()
ALGORITHMS = ["fcfs","sjf","srtf","priority","priority_preemptive","rr","hrrn","mlq","mlfq"]

def body():
    value = request.get_json()
    if not isinstance(value, dict): raise ValueError("Expected a JSON object")
    return value

def finite_json(value):
    if isinstance(value, float) and not math.isfinite(value): raise ValueError("Non-finite number")
    if isinstance(value, dict):
        for v in value.values(): finite_json(v)
    elif isinstance(value, list):
        for v in value: finite_json(v)

def validate_workload(v):
    if not isinstance(v, dict): raise ValueError("Scenario must be an object")
    v=copy.deepcopy(v)
    if not isinstance(v.get("name","Custom scenario"),str) or not 1<=len(v.get("name","Custom scenario"))<=120: raise ValueError("Invalid scenario name")
    if type(v.get("seed",42))is not int or not -(2**31)<=v.get("seed",42)<2**31: raise ValueError("Invalid scenario seed")
    if type(v.get("aging",False))is not bool: raise ValueError("Aging must be boolean")
    fs = v.get("flights")
    if not isinstance(fs, list) or not 1 <= len(fs) <= 100: raise ValueError("Use 1..100 flights")
    if v.get("algorithm", "fcfs") not in ALGORITHMS: raise ValueError("Unknown policy")
    if v.get("context", "airport") not in ["airport", "tasks"]: raise ValueError("Unknown scheduling context")
    ids = set()
    for f in fs:
        if not isinstance(f, dict) or not isinstance(f.get("id"), str) or not 1<=len(f["id"])<=36 or f["id"] in ids: raise ValueError("Unique flight IDs of 1..36 characters required")
        if len(f["id"].encode("utf-8"))>39 or "\x00" in f["id"]: raise ValueError("Flight ID exceeds native record bounds")
        ids.add(f["id"])
        for key, lo, hi, default in [("arrival",0,1000,0),("burst",1,100,5),("priority",0,99,3),("memory",1,65536,32),("departure",0,10000,60)]:
            x=f.get(key,default)
            if type(x) is not int or not lo<=x<=hi: raise ValueError(f"Invalid {key}")
        if "cancel_at" in f and (type(f["cancel_at"])is not int or not 0<=f["cancel_at"]<=10000): raise ValueError("Invalid cancellation timestamp")
        for key in ["airline","category"]:
            if key in f and (not isinstance(f[key],str) or len(f[key].encode("utf-8"))>(63 if key=="airline" else 23) or "\x00" in f[key]): raise ValueError(f"Invalid {key}")
        if "emergency" in f and type(f["emergency"]) is not bool: raise ValueError("Emergency must be boolean")
        req=f.get("requirements",[1,1,0,0,0,0])
        if not isinstance(req,list) or len(req)!=6 or any(type(x)is not int or not 0<=x<=100 for x in req): raise ValueError("Requirements must contain six nonnegative integers")
        if v.get("context","airport")=="airport" and req[0]!=1: raise ValueError("Airport flights must require exactly one runway")
    if sum(f.get("burst",5) for f in fs)>3000: raise ValueError("Execution demand is limited to 3000 ticks")
    resources=v.get("resources",{})
    if not isinstance(resources,dict): raise ValueError("Resources must be an object")
    for key in ["runways","gates","baggage","maintenance","fuel","crew","memory"]:
        if key in resources and (type(resources[key]) is not int or not (1 if key=="memory" else 0)<=resources[key]<=(65536 if key=="memory" else 100)): raise ValueError("Invalid resource capacity")
    if type(resources.get("max_flights",100))is not int or not 1<=resources.get("max_flights",100)<=100 or len(fs)>resources.get("max_flights",100): raise ValueError("Configured flight limit exceeded")
    for key in ["runway_classes","gate_classes"]:
        if key in resources and (not isinstance(resources[key],list) or any(x not in ["all","medium","heavy","light"] for x in resources[key])): raise ValueError("Invalid compatibility classes")
    faults=v.get("faults",[])
    if not isinstance(faults,list) or len(faults)>100: raise ValueError("Invalid faults")
    for f in faults:
        if not isinstance(f,dict): raise ValueError("Invalid fault")
        for k,lo,hi,d in [("time",0,1000,0),("duration",1,1000,10),("resource",0,5,0),("reduction",0,100,1)]:
            if type(f.get(k,d))is not int or not lo<=f.get(k,d)<=hi: raise ValueError("Invalid fault value")
    for k,lo,hi,d in [("quantum",1,100,2),("aging_interval",1,1000,5),("starvation_threshold",1,10000,15)]:
        if type(v.get(k,d))is not int or not lo<=v.get(k,d)<=hi: raise ValueError(f"Invalid {k}")
    finite_json(v)
    for key,value in dict(name="Custom scenario",seed=42,context="airport",algorithm="fcfs",quantum=2,aging=False,aging_interval=5,starvation_threshold=15,faults=[]).items(): v.setdefault(key,value)
    capacities=dict(runways=3,gates=6,baggage=4,maintenance=3,fuel=2,crew=5,memory=1024,max_flights=100)
    v.setdefault("resources",{})
    for key,value in capacities.items(): v["resources"].setdefault(key,value)
    v["resources"].setdefault("runway_classes",["all"]*v["resources"]["runways"])
    v["resources"].setdefault("gate_classes",["all"]*v["resources"]["gates"])
    for f in fs:
        for key,value in dict(airline="AeroOS",category="medium",arrival=0,burst=5,priority=3,emergency=False,memory=32,requirements=[1,1,0,0,0,0]).items(): f.setdefault(key,value)
        f.setdefault("departure",f["arrival"]+60)
    return v

def validate_lab(kind,v):
    def integer(x,lo,hi,label):
        if type(x)is not int or not lo<=x<=hi: raise ValueError("Invalid "+label)
    if kind in ["banker","deadlock"]:
        available=v.get("available");allocation=v.get("allocation");claims=v.get("maximum" if kind=="banker" else "request")
        if not isinstance(available,list) or not 1<=len(available)<=6: raise ValueError("Use 1..6 resource columns")
        for x in available: integer(x,0,10000,"available value")
        if not isinstance(allocation,list) or not 1<=len(allocation)<=100 or not isinstance(claims,list) or len(claims)!=len(allocation): raise ValueError("Invalid process matrices")
        for matrix in [allocation,claims]:
            for row in matrix:
                if not isinstance(row,list) or len(row)!=len(available): raise ValueError("Matrix shape mismatch")
                for x in row: integer(x,0,10000,"matrix value")
        for key in ["recover","safe_mode"]:
            if key in v and type(v[key])is not bool: raise ValueError(key+" must be boolean")
        if "victim" in v: integer(v["victim"],0,len(allocation)-1,"victim")
        if "request_process" in v:
            integer(v["request_process"],0,len(allocation)-1,"request process")
            vector=v.get("request_vector")
            if not isinstance(vector,list) or len(vector)!=len(available): raise ValueError("Request vector shape mismatch")
            for x in vector: integer(x,0,10000,"request value")
    elif kind=="memory":
        mode=v.get("mode","paging")
        if mode=="paging":
            integer(v.get("frames",3),1,64,"frame count");integer(v.get("page_size",256),1,65536,"page size")
            integer(v.get("logical_address",0),0,100000000,"logical address");integer(v.get("process_bytes",0),0,100000000,"process byte size")
            refs=v.get("references")
            if not isinstance(refs,list) or not 1<=len(refs)<=1000: raise ValueError("Use 1..1000 references")
            for x in refs: integer(x,0,1000000,"page reference")
        elif mode=="contiguous":
            integer(v.get("size",1024),1,65536,"memory size")
            ops=v.get("operations",[])
            if not isinstance(ops,list) or len(ops)>500: raise ValueError("Use at most 500 memory operations")
            for op in ops:
                if not isinstance(op,dict) or op.get("action","allocate") not in ["allocate","free","swap","compact"]: raise ValueError("Invalid memory operation")
                integer(op.get("process",0),0,1000000,"process index")
                if op.get("action","allocate")=="allocate": integer(op.get("size",64),1,v.get("size",1024),"allocation size")
        else: raise ValueError("Unknown memory mode")
    elif kind=="sync":
        if v.get("mode","mutex") not in ["mutex","binary","counting","race","producer_consumer","readers_writers","dining"]: raise ValueError("Unknown thread experiment")
        integer(v.get("threads",4),2,16,"thread count");integer(v.get("iterations",20),1,100,"iteration count")
        if v.get("mode")=="counting": integer(v.get("capacity",2),1,v.get("threads",4),"permit capacity")

def engine(kind, value):
    finite_json(value)
    validate_lab(kind,value)
    if kind in ["airport","schedule"]: validate_workload(value)
    result = subprocess.run([str(ROOT/"core"/"aeroos"),kind],input=json.dumps(value),text=True,capture_output=True,timeout=20,check=False)
    if result.returncode:
        try: message=json.loads(result.stdout).get("error","Engine rejected input")
        except (ValueError,AttributeError): message="Engine rejected invalid input"
        raise ValueError(message)
    return json.loads(result.stdout)

def persisted(kind,v):
    result=engine(kind,v)
    rid=database.save_run(str(v.get("name",kind))[:120],kind,v,result)
    result["run_id"]=rid
    return result

@app.before_request
def local_only():
    if request.host.split(":")[0] not in ["127.0.0.1","localhost","[::1]"]: return jsonify(error="Local host required"),403
    origin=request.headers.get("Origin")
    if origin and urlparse(origin).hostname not in ["127.0.0.1","localhost"]: return jsonify(error="Cross-site requests rejected"),403

@app.errorhandler(Exception)
def error(exc):
    if isinstance(exc,HTTPException): return jsonify(error=exc.description),exc.code
    if isinstance(exc,(ValueError,TypeError,KeyError,IndexError)): return jsonify(error=str(exc)),400
    if isinstance(exc,subprocess.TimeoutExpired): return jsonify(error="Engine time limit exceeded"),408
    app.logger.exception("Request failed")
    return jsonify(error="Internal error; see backend log"),500

@app.get("/api/bootstrap")
def bootstrap():
    with database.connect() as db: saved=[dict(r) for r in db.execute("SELECT id,name,created FROM scenarios ORDER BY id DESC")]
    return jsonify(scenarios=NAMES,default=scenario(),algorithms=ALGORITHMS,saved=saved,labs=dict(banker=BANKER,deadlock=DEADLOCK,paging=PAGING,contiguous=CONTIGUOUS))

@app.get("/api/scenario")
def builtin():
    name=request.args.get("name",NAMES[0]);seed=int(request.args.get("seed",42))
    if name not in NAMES+["Chaos Mode"]: raise ValueError("Unknown scenario")
    return jsonify(scenario(name,seed))

@app.post("/api/simulate")
def simulate():
    v=validate_workload(body())
    result=persisted("schedule" if v.get("context")=="tasks" else "airport",v)
    result["scenario"]=v
    return jsonify(result)

@app.post("/api/lab/<kind>")
def lab(kind):
    if kind not in ["banker","deadlock","memory","sync"]: raise ValueError("Unknown lab")
    v=body()
    if kind=="memory" and v.get("compare"):
        policies=["fifo","lru","optimal"] if v.get("mode","paging")=="paging" else ["first","best","worst","next"]
        measured=[dict(algorithm=a,output=engine(kind,{**v,"algorithm":a})) for a in policies]
        result=next((r["output"] for r in measured if r["algorithm"]==v.get("algorithm")),measured[0]["output"]).copy()
        result["comparisons"]=[dict(algorithm=r["algorithm"],**{k:x for k,x in r["output"].items() if k!="steps"}) for r in measured]
        result["run_id"]=database.save_run("Memory policy comparison",kind,v,result)
        return jsonify(result)
    return jsonify(persisted(kind,v))

@app.post("/api/verify")
def verify():
    v=body()
    capacities=v.get("capacities")
    plans=v.get("plans")
    if not isinstance(capacities,list) or not 1<=len(capacities)<=4 or any(type(x)is not int or not 1<=x<=6 for x in capacities): raise ValueError("Use 1..4 capacities, each 1..6")
    if not isinstance(plans,list) or not 1<=len(plans)<=6: raise ValueError("Use 1..6 process plans")
    for plan in plans:
        if not isinstance(plan,list) or not 1<=len(plan)<=6 or any(type(x)is not int or not 0<=x<len(capacities) for x in plan): raise ValueError("Invalid acquisition plan")
        if any(plan.count(r)>capacities[r] for r in range(len(capacities))): raise ValueError("Process claim exceeds capacity")
    if type(v.get("max_states",20000))is not int or not 1<=v.get("max_states",20000)<=50000: raise ValueError("Invalid exploration bound")
    return jsonify(persisted("modelcheck",v))

@app.post("/api/battle")
def battle():
    v=body();workload=validate_workload(v["scenario"]);algs=v.get("algorithms",ALGORITHMS)
    if not isinstance(algs,list) or not 1<=len(algs)<=9 or any(a not in ALGORITHMS for a in algs): raise ValueError("Select 1..9 policies")
    results=[]
    for algorithm in algs:
        s=copy.deepcopy(workload);s.update(algorithm=algorithm,context="tasks")
        r=engine("schedule",s);results.append(dict(algorithm=algorithm,metrics=r["metrics"],gantt=r["gantt"],flights=r["flights"]))
    output=dict(results=results,assumption="Identical arrival/burst workloads on one abstract CPU; resources excluded")
    output["run_id"]=database.save_run("Scheduler Battle Arena","battle",v,output)
    return jsonify(output)

@app.post("/api/experiment")
def experiment():
    v=body();baseline=validate_workload(v["baseline"]);modified=validate_workload(v["modified"])
    a=engine("schedule" if baseline.get("context")=="tasks" else "airport",baseline)
    b=engine("schedule" if modified.get("context")=="tasks" else "airport",modified)
    out=dict(baseline=a,modified=b,differences={k:b["metrics"][k]-x for k,x in a["metrics"].items() if type(x)in(int,float)})
    out["run_id"]=database.save_run("What-if experiment","experiment",v,out)
    return jsonify(out)

@app.route("/api/scenarios",methods=["GET","POST"])
def scenarios():
    with database.connect() as db:
        if request.method=="GET": return jsonify([dict(r) for r in db.execute("SELECT id,name,created FROM scenarios ORDER BY id DESC")])
        v=body();payload=validate_workload(v["scenario"]);name=str(v.get("name",payload.get("name","Untitled")))[:120]
        engine("schedule" if payload.get("context")=="tasks" else "airport",payload)
        sid=db.execute("INSERT INTO scenarios(name,payload) VALUES(?,?)",(name,json.dumps(payload))).lastrowid
        return jsonify(id=sid,name=name),201

@app.route("/api/scenarios/<int:sid>",methods=["GET","DELETE"])
def saved(sid):
    with database.connect() as db:
        row=db.execute("SELECT * FROM scenarios WHERE id=?",(sid,)).fetchone()
        if not row: return jsonify(error="Scenario not found"),404
        if request.method=="DELETE": db.execute("DELETE FROM scenarios WHERE id=?",(sid,));return jsonify(deleted=sid)
        return jsonify(json.loads(row["payload"]))

@app.get("/api/history")
def history():
    with database.connect() as db: return jsonify([dict(r) for r in db.execute("SELECT id,name,kind,created FROM runs ORDER BY id DESC LIMIT 100")])

@app.get("/api/runs/<int:rid>")
def run(rid):
    with database.connect() as db: row=db.execute("SELECT * FROM runs WHERE id=?",(rid,)).fetchone()
    if not row: return jsonify(error="Run not found"),404
    return jsonify(id=rid,kind=row["kind"],input=json.loads(row["input"]),output=json.loads(row["output"]))

@app.route("/api/snapshots",methods=["GET","POST"])
def snapshots():
    with database.connect() as db:
        if request.method=="GET": return jsonify([dict(r) for r in db.execute("SELECT * FROM snapshots ORDER BY id DESC")])
        v=body();rid=v.get("run_id");pos=v.get("position")
        row=db.execute("SELECT output FROM runs WHERE id=?",(rid,)).fetchone()
        if not row: raise ValueError("Run not found")
        snaps=json.loads(row[0]).get("snapshots",[])
        if type(pos)is not int or not 0<=pos<len(snaps): raise ValueError("Invalid snapshot position")
        sid=db.execute("INSERT INTO snapshots(name,run_id,position) VALUES(?,?,?)",(str(v.get("name","Snapshot"))[:120],rid,pos)).lastrowid
        return jsonify(id=sid),201

@app.get("/api/reports/<int:rid>")
def report(rid):
    with database.connect() as db: row=db.execute("SELECT * FROM runs WHERE id=?",(rid,)).fetchone()
    if not row: return jsonify(error="Run not found"),404
    data=json.loads(row["output"]);inputs=json.loads(row["input"])
    if request.args.get("format")=="json": return Response(json.dumps(dict(input=inputs,output=data),indent=2),mimetype="application/json",headers={"Content-Disposition":f"attachment; filename=aeroos-{rid}.json"})
    if request.args.get("format")=="csv":
        output=io.StringIO();writer=csv.writer(output);writer.writerow(["Field","Value"])
        def flatten(value,path=""):
            if isinstance(value,dict):
                for k,x in value.items():
                    if k not in ["snapshots","events","gantt","steps"]: flatten(x,f"{path}.{k}" if path else k)
            elif isinstance(value,list):
                for i,x in enumerate(value): flatten(x,f"{path}[{i}]")
            else:
                if isinstance(value,str) and value.startswith(("=","+","-","@")): value="'"+value
                writer.writerow([path,value])
        flatten(data)
        return Response(output.getvalue(),mimetype="text/csv",headers={"Content-Disposition":f"attachment; filename=aeroos-{rid}.csv"})
    text=f"# AeroOS Simulation Report\n\nRun {rid} · {row['name']} · {row['created']}\n\nContext: {row['kind']}\n\n## Reproducible input\n\n```json\n{json.dumps(inputs,indent=2)}\n```\n\n## Measured results\n\n```json\n{json.dumps({k:v for k,v in data.items() if k not in ['snapshots','events','gantt']},indent=2)}\n```\n\n## Interpretation\n\nWaiting time includes ready and resource-blocked intervals. Scheduler starvation counts exclude resource wait. Throughput is completed tasks divided by elapsed simulated ticks. Resource utilization uses configured slot capacity, including closures as unavailable idle time. Airport operations are non-preemptive. Unsafe Banker states are not proof of deadlock. Concurrency logs represent an actual pthread run, whose ordering may vary.\n\nCompare policies on identical inputs; no algorithm is universally best. Full recorded state is available in the JSON export.\n"
    return Response(text,mimetype="text/markdown",headers={"Content-Disposition":f"attachment; filename=aeroos-{rid}.md"})

@app.get("/api/audit")
def implementation_audit():
    return send_from_directory(ROOT/"docs","audit-2026-10-26.md",as_attachment=True)

@app.route("/api/<path:unknown>",methods=["GET","POST","DELETE"])
def unknown_api(unknown):
    return jsonify(error="Unknown API route"),404

@app.get("/")
@app.get("/<path:path>")
def frontend(path="index.html"):
    dist=ROOT/"frontend"/"dist"
    if not (dist/"index.html").exists(): return "Build frontend first: see README",503
    return send_from_directory(dist,path if (dist/path).is_file() else "index.html")

if __name__=="__main__":
    app.run(host="127.0.0.1",port=5050,debug=False,threaded=True)
