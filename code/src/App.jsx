import { useState } from 'react';
import GaugesPanel from './components/GaugesPanel';
import MotorPanel  from './components/MotorPanel';
import { useWebSocket } from './hooks/useWebSocket';

const CONN_META = {
  demo:       { dot: '#f59e0b', text: 'Demo mode' },
  connecting: { dot: '#60a5fa', text: 'Connecting…' },
  live:       { dot: '#4ade80', text: 'Live' },
  error:      { dot: '#f87171', text: 'Connection failed — reverting to demo' },
};

export default function App() {
  const [tab, setTab] = useState('sensors');
  const [ip,  setIp]  = useState('');

  const {
    sensors,
    motorStatus,
    connStatus,
    connect,
    sendCmd,
    startDemo,
  } = useWebSocket();

  const meta = CONN_META[connStatus];

  return (
    <div className="app">
      <header className="header">
        <div className="header-left">
          <span className="logo">SCS</span>
          <div>
            <p className="brand">Smart Compression Sleeve</p>
            <p className="conn-status">
              <span className="status-dot" style={{ background: meta.dot }} />
              {meta.text}
              {connStatus === 'live' && ` · ${ip}`}
            </p>
          </div>
        </div>

        <div className="conn-row">
          <input
            className="ip-input"
            value={ip}
            onChange={(e) => setIp(e.target.value)}
            onKeyDown={(e) => e.key === 'Enter' && connect(ip)}
            placeholder="192.168.x.x"
            inputMode="decimal"
            aria-label="ESP32 IP address"
          />
          <button className="btn-sm"             onClick={() => connect(ip)}>Connect</button>
          <button className="btn-sm btn-sm--muted" onClick={startDemo}>Demo</button>
        </div>
      </header>

      <nav className="tabs" role="tablist">
        <button
          role="tab"
          className={`tab ${tab === 'sensors' ? 'tab--active' : ''}`}
          onClick={() => setTab('sensors')}
          aria-selected={tab === 'sensors'}
        >
          Pressure
        </button>
        <button
          role="tab"
          className={`tab ${tab === 'motor' ? 'tab--active' : ''}`}
          onClick={() => setTab('motor')}
          aria-selected={tab === 'motor'}
        >
          Motor
        </button>
      </nav>

      {tab === 'sensors' && <GaugesPanel sensors={sensors} />}
      {tab === 'motor'   && <MotorPanel  motorStatus={motorStatus} sendCmd={sendCmd} />}
    </div>
  );
}
