import { useState, useEffect, useRef, useCallback } from 'react';
import { FSR_PINS } from '../utils/gaugeUtils';

const DEMO_INTERVAL_MS = 300;

function generateDemoSensors(t) {
  const base = 28 + Math.sin(t) * 9;
  const offsets = [1.3, 0.7, 1.1, 0.9];
  const amps    = [3.5, 4.0, 2.5, 3.0];
  return FSR_PINS.map((_, i) =>
    +Math.max(0, base + Math.sin(t * offsets[i]) * amps[i]).toFixed(1)
  );
}

export function useWebSocket() {
  const [sensors,     setSensors]     = useState([0, 0, 0, 0]);
  const [motorStatus, setMotorStatus] = useState('Idle');
  const [connStatus,  setConnStatus]  = useState('demo');

  const ws         = useRef(null);
  const demoTimer  = useRef(null);
  const demoT      = useRef(0);
  const demoPaused = useRef(false);

  const startDemo = useCallback(() => {
    ws.current?.close();
    ws.current = null;
    clearInterval(demoTimer.current);
    setConnStatus('demo');
    demoT.current = 0;
    demoPaused.current = false;
    demoTimer.current = setInterval(() => {
      if (demoPaused.current) return; // frozen during manual override
      demoT.current += 0.06;
      setSensors(generateDemoSensors(demoT.current));
    }, DEMO_INTERVAL_MS);
  }, []);

  const pauseDemo  = useCallback(() => { demoPaused.current = true;  }, []);
  const resumeDemo = useCallback(() => { demoPaused.current = false; }, []);

  const connect = useCallback((ip) => {
    if (!ip.trim()) return;
    clearInterval(demoTimer.current);
    ws.current?.close();
    setConnStatus('connecting');

    try {
      const socket = new WebSocket(`ws://${ip.trim()}/ws`);

      socket.onopen = () => setConnStatus('live');

      socket.onclose = () => {
        setConnStatus('error');
        startDemo();
      };

      socket.onerror = () => {
        setConnStatus('error');
      };

      socket.onmessage = (e) => {
        try {
          const d = JSON.parse(e.data);
          if (Array.isArray(d.s) && d.s.length === 4) setSensors(d.s);
          if (d.st) setMotorStatus(d.st);
        } catch { /* malformed frame — ignore */ }
      };

      ws.current = socket;
    } catch {
      setConnStatus('error');
      startDemo();
    }
  }, [startDemo]);

  const sendCmd = useCallback((cmd) => {
    const labels = { contract: 'Contracting…', retract: 'Retracting…', stop: 'Stopped' };
    setMotorStatus(labels[cmd] ?? cmd);
    if (ws.current?.readyState === WebSocket.OPEN) {
      ws.current.send(cmd);
    }
  }, []);

  useEffect(() => {
    startDemo();
    return () => {
      clearInterval(demoTimer.current);
      ws.current?.close();
    };
  }, [startDemo]);

  return { sensors, motorStatus, connStatus, connect, sendCmd, startDemo, pauseDemo, resumeDemo };
}
