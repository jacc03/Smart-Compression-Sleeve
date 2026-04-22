import PressureGauge from './PressureGauge';
import { FSR_PINS, getStatus, STATUS_COLOR, TARGET_MIN, TARGET_MAX } from '../utils/gaugeUtils';

const STATUS_LABEL = { ok: 'Target Met', low: 'Below Target', high: 'Above Target' };

export default function GaugesPanel({ sensors, manualOpen, onToggleManual, sendCmd }) {
  const avg    = sensors.reduce((a, b) => a + b, 0) / sensors.length;
  const status = getStatus(avg);
  const color  = STATUS_COLOR[status];

  // Warning toast — only shown when auto-control is active and out of range
  const showTightening = !manualOpen && avg < TARGET_MIN;
  const showReleasing  = !manualOpen && avg > TARGET_MAX;

  return (
    <>
      {/* ── Fixed warning toasts — never shift layout ── */}
      {showTightening && (
        <div className="toast toast--tightening">
          <span className="toast-icon">⚠</span>
          <div className="toast-text">
            <span className="toast-title">Tightening</span>
            <span className="toast-sub">Avg {avg.toFixed(1)} mmHg — below {TARGET_MIN} mmHg</span>
          </div>
        </div>
      )}
      {showReleasing && (
        <div className="toast toast--releasing">
          <span className="toast-icon">⚠</span>
          <div className="toast-text">
            <span className="toast-title">Releasing</span>
            <span className="toast-sub">Avg {avg.toFixed(1)} mmHg — above {TARGET_MAX} mmHg</span>
          </div>
        </div>
      )}

      {/* ── Main panel — layout never moves ── */}
      <div className="panel">
        <div className="gauges-grid">
          {sensors.map((v, i) => (
            <PressureGauge key={i} value={v} pin={FSR_PINS[i]} />
          ))}
        </div>

        <div className="avg-card">
          <div>
            <p className="avg-label">Average Pressure</p>
            <p className="avg-val" style={{ color }}>
              {avg.toFixed(1)}<span className="avg-unit"> mmHg</span>
            </p>
          </div>
          <span className={`badge badge--${status}`}>{STATUS_LABEL[status]}</span>
        </div>

        <div className="channel-strip">
          {sensors.map((v, i) => {
            const s = getStatus(v);
            return (
              <div key={i} className="channel">
                <div className="channel-track">
                  <div
                    className="channel-fill"
                    style={{ height: `${(v / 50) * 100}%`, background: STATUS_COLOR[s] }}
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

        <button
          className={`emergency-btn ${manualOpen ? 'emergency-btn--active' : ''}`}
          onClick={onToggleManual}
          aria-pressed={manualOpen}
        >
          <span className="emergency-btn-icon">⚡</span>
          Manual Override
        </button>

        <p className="hint">Target {TARGET_MIN}–{TARGET_MAX} mmHg · green band on each bar</p>
      </div>

      {/* ── Manual override fullscreen takeover ── */}
      {manualOpen && (
        <div className="manual-overlay">
          <div className="manual-overlay-header">
            <div>
              <p className="manual-overlay-title">Manual Override</p>
              <p className="manual-overlay-sub">Auto-control paused · Hold to run</p>
            </div>
            <button className="manual-close" onClick={onToggleManual}>✕</button>
          </div>

          <div className="manual-overlay-gauges">
            <div className="avg-card">
              <div>
                <p className="avg-label">Average Pressure</p>
                <p className="avg-val" style={{ color }}>
                  {avg.toFixed(1)}<span className="avg-unit"> mmHg</span>
                </p>
              </div>
              <span className={`badge badge--${status}`}>{STATUS_LABEL[status]}</span>
            </div>
          </div>

          <div className="manual-btns">
            <button
              className="manual-btn manual-btn--contract"
              onPointerDown={() => sendCmd('contract')}
            >
              <span className="manual-btn-arrow">▲</span>
              Contract
            </button>
            <button
              className="manual-btn manual-btn--retract"
              onPointerDown={() => sendCmd('retract')}
            >
              <span className="manual-btn-arrow">▼</span>
              Retract
            </button>
          </div>

          <button className="manual-resume" onClick={onToggleManual}>
            ✓ Resume Auto-Control
          </button>
        </div>
      )}
    </>
  );
}
