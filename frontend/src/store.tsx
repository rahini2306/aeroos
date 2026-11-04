import { createContext, useContext } from 'react';
import type { Scenario, Run, Snapshot } from './types';
export type Page = 'home'|'airport'|'scheduler'|'safety'|'workbench';
export interface Session {
  boot: any; scenario?: Scenario; run?: Run; current?: Snapshot;
  position: number; setPosition: (n:number)=>void;
  playing: boolean; setPlaying: (p:boolean)=>void;
  speed: number; setSpeed: (n:number)=>void;
  busy: boolean; page: Page; open: (p:Page)=>void;
  startRun: (s:Scenario, seek?:number)=>Promise<void>;
  act: (fn:()=>Promise<void>)=>Promise<void>;
  notify: (s:string)=>void;
}
export const SessionContext=createContext<Session>(null!);
export const useSession=()=>useContext(SessionContext);
