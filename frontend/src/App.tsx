import {useEffect,useState} from 'react';
import {api, type Run, type Scenario} from './types';
import {SessionContext, type Page} from './store';
import {Desktop, folders, FolderIcon} from './Desktop';
import {Airport} from './Airport';
import {Scheduler} from './Scheduler';
import {Safety} from './Safety';
import {Workbench} from './Workbench';
export default function App(){
  const [page,setPage]=useState<Page>('home');
  const [boot,setBoot]=useState<any>();
  const [scenario,setScenario]=useState<Scenario>();
  const [run,setRun]=useState<Run>();
  const [position,setPosition]=useState(0);
  const [playing,setPlaying]=useState(false);
  const [speed,setSpeed]=useState(2);
  const [busy,setBusy]=useState(false);
  const [error,setError]=useState('');
  const [notice,setNotice]=useState('');
  const [expanded,setExpanded]=useState(false);
  const notify=(s:string)=>setNotice(s);
  const act=async(fn:()=>Promise<void>)=>{setBusy(true);setError('');setNotice('');try{await fn()}catch(e){setError(e instanceof Error?e.message:String(e))}finally{setBusy(false)}};
  const startRun=async(s:Scenario,seek=0)=>{
    setPlaying(false);
    const r=await api<Run>('/simulate',s);
    setRun(r);setScenario(r.scenario??s);
    setPosition(Math.max(0,Math.min(seek,r.snapshots.length-1)));
    if(r.metrics.incomplete)notify('Some flights cannot finish with these resources. Inspect the blocked queue.');
  };
  useEffect(()=>{void act(async()=>{const b=await api('/bootstrap');setBoot(b);await startRun(b.default)})},[]);
  useEffect(()=>{
    if(!playing||!run)return;
    const timer=setInterval(()=>setPosition(p=>{if(p>=run.snapshots.length-1){setPlaying(false);return p}return p+1}),1000/speed);
    return()=>clearInterval(timer);
  },[playing,speed,run]);
  const current=run?.snapshots[position];
  const open=(p:Page)=>{setPage(p);setError('');setNotice('');window.scrollTo(0,0)};
  const context={boot,scenario,run,current,position,setPosition,playing,setPlaying,speed,setSpeed,busy,page,open,startRun,act,notify};
  const title=folders.find(f=>f.id===page)?.name??'Desktop';
  return <SessionContext.Provider value={context}>
    <div className="paper-desktop">
      <header className="desktop-menu"><button className="wordmark" onClick={()=>open('home')}>a<span>o</span></button><span>AeroOS <i>/</i> Airport operating system</span><div className="menu-right"><span className={'connection '+(error?'has-error':'')}>{busy?'working…':error?'needs attention':run?'engine connected':'connecting…'}</span></div></header>
      {page==='home'?<><Desktop/>{error&&<div className="home-alert" role="alert">{error}<button onClick={()=>window.location.reload()}>Retry</button></div>}</>:<div className={'workspace '+(expanded?'expanded':'')}>
        <nav className="folder-rail" aria-label="Folders">{folders.map(f=><button className={'desktop-folder '+(page===f.id?'selected':'')} aria-current={page===f.id?'page':undefined} key={f.id} onClick={()=>open(f.id)}><FolderIcon color={f.id==='safety'?'#e7b98e':'#f0d777'}/><strong>{f.name}</strong></button>)}</nav>
        <section className="app-window" aria-label={title+' window'}><div className="window-title"><span>A:\AEROOS\{page}.exe</span><div><button onClick={()=>setExpanded(!expanded)} aria-label={expanded?'Restore window':'Maximize window'}>{expanded?'▣':'□'}</button><button className="window-close" onClick={()=>open('home')} aria-label="Close window">×</button></div></div>
          <div className="window-body"><div className="page-topline"><div><span className="section-number">0{folders.findIndex(f=>f.id===page)+1} / AEROOS</span><h1>{title}</h1></div><span className="page-tick">{current?'T+'+String(current.time).padStart(3,'0'):'—'}</span></div>
            {error&&<div className="message error" role="alert">{error}<button onClick={()=>setError('')}>×</button></div>}{notice&&<div className="message" role="status">{notice}<button onClick={()=>setNotice('')}>×</button></div>}
            {!boot||!scenario||!run?<p className="quiet empty">Connecting to the C engine…</p>:page==='airport'?<Airport/>:page==='scheduler'?<Scheduler/>:page==='safety'?<Safety/>:<Workbench/>}
          </div></section>
      </div>}
      <footer className="desktop-dock"><button onClick={()=>open('home')} aria-label="Desktop"><span className="dock-home">ao</span></button><span className="dock-divider"/>{folders.slice(0,3).map(f=><button className={page===f.id?'active':''} key={f.id} onClick={()=>open(f.id)} aria-label={'Open '+f.name}><FolderIcon color={f.id==='safety'?'#e7b98e':'#f0d777'}/></button>)}<span className="dock-divider"/><span className="dock-caption">{page==='home'?'Choose a folder to begin.':title+' / '+(playing?'playing':'ready')}</span><span className="dock-version">LOCAL EDITION · 02</span></footer>
    </div>
  </SessionContext.Provider>;
}
