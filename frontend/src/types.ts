export type Flight = {id:string;airline:string;category:string;arrival:number;departure:number;burst:number;priority:number;emergency:boolean;memory:number;cancel_at?:number;requirements:number[]};
export type Scenario = {name:string;seed:number;context:string;algorithm:string;quantum:number;aging:boolean;aging_interval:number;starvation_threshold:number;resources:Record<string,number|string[]>;flights:Flight[];faults:{time:number;duration:number;resource:number;reduction:number}[]};
export type Event = {process:number;from?:string;to:string;reason:string;algorithm?:string;alternatives?:number[]};
export type Snapshot = {time:number;memory_used:number;processes:number[][];owners:number[][];capacity:number[];queues:number[];events:Event[]};
export type Slice = {process:number;start:number;end:number;lane:number};
export type Run = {scenario?:Scenario;run_id:number;context:string;algorithm:string;snapshots:Snapshot[];gantt:Slice[];flights:Record<string,any>[];metrics:Record<string,number|boolean>;resource_metrics:Record<string,any>[]};
export async function api<T=any>(path:string,data?:unknown,method?:string):Promise<T>{const res=await fetch('/api'+path,{method:method||(data?'POST':'GET'),headers:data?{'Content-Type':'application/json'}:{},body:data?JSON.stringify(data):undefined});const result=await res.json();if(!res.ok)throw new Error(result.error||'Request failed');return result;}
export const states=['NEW','READY','RUNNING','WAITING','BLOCKED','COMPLETED','CANCELLED'];
export const resources=['runways','gates','baggage','maintenance','fuel','crew'];
export const labels:Record<string,string>={fcfs:'FCFS',sjf:'SJF',srtf:'SRTF',priority:'Priority · non-preemptive',priority_preemptive:'Priority · preemptive',rr:'Round Robin',hrrn:'HRRN',mlq:'Multilevel Queue',mlfq:'MLFQ'};
export const color=['#718151','#b67d51','#7d99a0','#ab7d84','#8a885d','#7a8772','#ad9271'];
export const fmt=(v:unknown)=>typeof v==='number'?(Number.isInteger(v)?String(v):v.toFixed(2)):String(v??'—');
