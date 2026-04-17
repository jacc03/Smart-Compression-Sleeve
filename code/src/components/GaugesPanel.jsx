import PressureGauge from './PressureGauge';
import { FSR_PINS, getStatus, STATUS_COLOR, TARGET_MIN, TARGET_MAX } from '../utils/gaugeUtils';

const STATUS_LABEL = { ok: 'Target Met', low: 'Below Target', high: 'Above Target' };

function MotorWarning({ avg }) {
  if (avg < TARGET_MIN) {
    return (
      <div className="motor-warning motor-warning--tightening">
        <span className="motor-warning-icon">⚠</span>
        <div className="motor-warning-text">
          <span className="motor-warning-title">Tightening</span>
          <span className="motor-warning-sub">
            Avg {avg.toFixed(1)} mmHg — below {TARGET_MIN} mmHg target
          </span>
        </div>
      </div>
    );
  }
  if (avg > TARGET_MAX) {
    return (
      <div className="motor-warning motor-warning--releasing">
        <span className="motor-warning-icon">⚠</span>
        <div className="motor-warning-text">
          <span className="motor-warning-title">Releasing</span>
          <span className="motor-warning-sub">
            Avg {avg.toFixed(1)} mmHg — above {TARGET_MAX} mmHg target
          </span>
        </div>
      </div>
    );
  }
  return null;
}

export default function GaugesPanel({ sensors }) {
  const avg    = sensors.reduce((a, b) => a + b, 0) / sensors.length;
  const status = getStatus(avg);
  const color  = STATUS_COLOR[status];

  return (
    <div className="panel">
      {/* Motor warning — shown when auto-control is active */}
      <MotorWarning avg={avg} />

      {/* 4-channel gauges */}
      <div className="gauges-grid">
        {sensors.map((v, i) => (
          <PressureGauge key={i} value={v} pin={FSR_PINS[i]} />
        ))}
      </div>

      {/* Average readout */}
      <div className="avg-card">
        <div>
          <p className="avg-label">Average Pressure</p>
          <p className="avg-val" style={{ color }}>
            {avg.toFixed(1)}<span className="avg-unit"> mmHg</span>
          </p>
        </div>
        <span className={`badge badge--${status}`}>{STATUS_LABEL[status]}</span>
      </div>

      {/* Mini channel strip (vertical bar per sensor) */}
      <div className="channel-strip">
        {sensors.map((v, i) => {
          const s = getStatus(v);
          return (
            <div key={i} className="channel">
              <div className="channel-track">
                <div
                  className="channel-fill"
                  style={{
                    height: `${(v / 50) * 100}%`,
                    background: STATUS_COLOR[s],
                  }}
                />
                <div
                  className="channel-target"
                  style={{
                    bottom: `${(TARGET_MIN / 50) * 100}%`,
                    height: `${((TARGET_MAX - TARGET_MIN) / 50) * 100}%`,
                  }}
                />
              </div>
              <span className="channel-num">{v.toFixed(0)}</span>
            </div>
          );
        })}
      </div>

      <p className="hint">
        Target {TARGET_MIN}–{TARGET_MAX} mmHg · green band on each bar
      </p>
    </div>
  );
}
