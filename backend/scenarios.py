"""Seeded educational workloads. No simulation results are embedded here."""
import random

NAMES = ["Normal Airport Operations", "Peak Hour Congestion", "Emergency Priority", "Runway Failure", "Gate Shortage", "Resource Deadlock", "Starvation Scenario", "Memory Pressure", "Synchronization Stress Test"]

def scenario(name=NAMES[0], seed=42):
    rng = random.Random(seed)
    resources = dict(runways=3, gates=6, baggage=4, maintenance=3, fuel=2, crew=5, memory=1024, max_flights=100, runway_classes=["all"]*3, gate_classes=["all"]*6)
    flights = []
    busy = name in [NAMES[1], "Chaos Mode"]
    count = 30 if busy else 14
    carriers = [("AI", "Air India"), ("6E", "IndiGo"), ("EK", "Emirates"), ("SQ", "Singapore Airlines"), ("UK", "Vistara")]
    for i in range(count):
        prefix, airline = carriers[i % len(carriers)]
        arrival = rng.randrange(0, 12 if busy else 30)
        flights.append(dict(id=f"{prefix}{200+i*13}", airline=airline, category="heavy" if i%5==0 else "medium", arrival=arrival, departure=arrival+50, burst=rng.randint(3, 9), priority=rng.randint(1, 8), emergency=(name in [NAMES[2], "Chaos Mode"] and i%7==0), memory=rng.choice([32, 64, 96]), requirements=[1,1,int(i%2==0),int(i%4==0),int(i%3==0),1]))
    result = dict(name=name, seed=seed, context="airport", algorithm="priority" if name==NAMES[2] else "fcfs", quantum=2, aging=False, aging_interval=5, starvation_threshold=15, resources=resources, flights=flights, faults=[])
    if name==NAMES[3] or name=="Chaos Mode":
        result["faults"]=[dict(time=5,duration=20,resource=0,reduction=1)]
    if name==NAMES[4] or name=="Chaos Mode": resources["gates"]=2
    if name==NAMES[6]:
        result.update(context="tasks", algorithm="priority")
        flights[0].update(arrival=0,burst=30,priority=9)
        for i, f in enumerate(flights[1:]): f.update(arrival=i,burst=3,priority=1)
    if name==NAMES[7]: resources["memory"]=128
    if name=="Chaos Mode": resources.update(runways=2,fuel=1,baggage=1)
    return result

BANKER = dict(available=[3,3,2],maximum=[[7,5,3],[3,2,2],[9,0,2],[2,2,2],[4,3,3]],allocation=[[0,1,0],[2,0,0],[3,0,2],[2,1,1],[0,0,2]])
DEADLOCK = dict(available=[0,0], allocation=[[1,0],[0,1]],request=[[0,1],[1,0]])
PAGING = dict(mode="paging",algorithm="lru",frames=3,page_size=256,references=[7,0,1,2,0,3,0,4,2,3,0,3,2],logical_address=512)
CONTIGUOUS = dict(mode="contiguous",algorithm="first",size=1024,operations=[dict(process=0,size=180),dict(process=1,size=220),dict(process=2,size=100),dict(action="free",process=1),dict(process=3,size=150)])
