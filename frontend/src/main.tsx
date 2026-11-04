import React from 'react';
import {createRoot} from 'react-dom/client';
import App from './App';
import './styles.css';
class ErrorBoundary extends React.Component<{children:React.ReactNode},{error:string}>{
  state={error:''};
  static getDerivedStateFromError(error:Error){return {error:error.message}}
  render(){return this.state.error?<div className="recovery-screen"><h1>AeroOS</h1><p>{this.state.error}</p><button onClick={()=>window.location.reload()}>Reopen desktop</button></div>:this.props.children}
}
createRoot(document.getElementById('root')!).render(<ErrorBoundary><App/></ErrorBoundary>);
