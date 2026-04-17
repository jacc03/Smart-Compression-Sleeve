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

/**
 * Manages WebSocket connection to the ESP32 and demo mode fallback.
 *
 * Returns:
 *   sensors      – float[4], current FSR readings (mmHg)
 *   motorStatus  – string, last status from ESP32 or local label
 *   connStatus   – 'demo' | 'connecting' | 'live' | 'error'
 *   connect(ip)  – open WebSocket to ws://<ip>/ws
 *   sendCmd(cmd) – send 'contract' | 'retract' | 'stop'
 *   startDemo()  – revert to simulated data
 */
export function useWebSocket() {
  const [sensors,     setSensors]     = useState([0, 0, 0, 0]);
  const [motorStatus, setMotorStatus] = useState('Idle');
  const [connStatus,  setConnStatus]  = useState('demo');

  const ws        = useRef(null);
  const demoTimer = useRef(null);
  const demoT     = useRef(0);

  const startDemo = useCallback(() => {
    ws.current?.close();
    ws.current = null;
    clearInterval(demoTimer.current);
    setConnStatus('demo');
    demoT.current = 0;
    demoTimer.current = setInterval(() => {
      demoT.current += 0.06;
      setSensors(generateDemoSensors(demoT.current));
    }, DEMO_INTERVAL_MS);
  }, []);

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

  return { sensors, motorStatus, connStatus, connect, sendCmd, startDemo };
}
