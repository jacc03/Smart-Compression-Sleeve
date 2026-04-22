import { useState } from 'react';
import GaugesPanel from './components/GaugesPanel';
import { useWebSocket } from './hooks/useWebSocket';

const CONN_META = {
  demo:       { dot: '#f59e0b', text: 'Demo mode' },
  connecting: { dot: '#60a5fa', text: 'Connecting…' },
  live:       { dot: '#4ade80', text: 'Live' },
  error:      { dot: '#f87171', text: 'Connection failed — reverting to demo' },
};

export default function App() {
  const [ip,         setIp]         = useState('');
  const [manualOpen, setManualOpen] = useState(false);

  const { sensors, connStatus, connect, sendCmd, startDemo, pauseDemo, resumeDemo } = useWebSocket();

  const meta = CONN_META[connStatus];

  const toggleManual = () => {
    if (manualOpen) {
      sendCmd('resume'); // exit manual mode, re-enable auto-control
      resumeDemo();
    } else {
      sendCmd('pause');  // enter manual mode, freeze auto-control
      pauseDemo();
    }
    setManualOpen(prev => !prev);
  };

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
            placeholder="192.168.4.1"
            inputMode="decimal"
            aria-label="ESP32 IP address"
          />
          <button className="btn-sm"                onClick={() => connect(ip)}>Connect</button>
          <button className="btn-sm btn-sm--muted"   onClick={startDemo}>Demo</button>
        </div>
      </header>

      <GaugesPanel
        sensors={sensors}
        manualOpen={manualOpen}
        onToggleManual={toggleManual}
        sendCmd={sendCmd}
      />
    </div>
  );
}
