import { useSession, type Page } from './store';
export const folders:{id:Page;name:string;caption:string}[]=[
  {id:'airport',name:'Airport',caption:'Run, replay & inspect'},
  {id:'scheduler',name:'Scheduler',caption:'Compare the same workload'},
  {id:'safety',name:'Safety lab',caption:'Explore every interleaving'},
  {id:'workbench',name:'Workbench',caption:'Memory & real threads'},
];
export function FolderIcon({color='#f3d977'}:{color?:string}) {
  return <svg width="66" height="57" viewBox="0 0 80 68" aria-hidden="true">
    <path d="M6 17 Q6 11 12 11 H30 L36 17 H68 Q73 17 73 22 V57 H6Z" fill={color} stroke="currentColor" strokeWidth="2"/>
    <path d="M11 16 H29 L34 22 H65 V54 H11Z" fill="#fff8de" stroke="currentColor" strokeWidth="1.5"/>
    <path d="M17 25 H56 V48 H17Z" fill="#f6f3e9" stroke="currentColor" strokeWidth="1.2"/>
    <path d="M21 28 H43 V34 H21Z" fill="#83a2a7" stroke="currentColor"/>
    <path d="M5 32 H32 L37 26 H76 L64 61 H1Z" fill={color} stroke="currentColor" strokeWidth="2" strokeLinejoin="round"/>
    <path d="M11 18 H25" stroke="#fff7d2" strokeWidth="2"/>
  </svg>;
}
export function Desktop(){
  const {open}=useSession();
  return <section className="desktop-home" aria-label="AeroOS desktop">
    <div className="edition">AIRPORT OPERATIONS / OS EXPERIMENTS</div>
    <div className="desktop-title"><span className="title-small">a small airport.</span><h1>AeroOS</h1><p>A place to run, question, and verify operating systems.</p></div>
    <div className="home-folders">{folders.map((f,i)=><button className={'desktop-folder home-folder folder-'+i} key={f.id} onClick={()=>open(f.id)}><FolderIcon color={i===2?'#e7b98e':'#f0d777'}/><strong>{f.name}</strong><small>{f.caption}</small>{i===2&&<span className="folder-note">NEW EXPERIMENT</span>}</button>)}</div>
    <button className="desktop-postcard" onClick={()=>open('safety')}><span className="postcard-number">01—</span><div><h2>What if a different<br/>thread ran first?</h2><p>Find the failure. Then check the fix.</p></div><span className="postcard-arrow">↗</span></button>
    <div className="desktop-footnote"><span>BUILT IN C · REPLAYED IN YOUR BROWSER</span><span>Educational model, not aviation guidance.</span></div>
  </section>;
}
