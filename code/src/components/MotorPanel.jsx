const PIN_ROWS = [
  { label: 'AIN1',   value: 'GPIO 25' },
  { label: 'AIN2',   value: 'GPIO 26' },
  { label: 'SLP',    value: 'GPIO 27' },
  { label: 'Driver', value: 'DRV8833' },
];

export default function MotorPanel({ motorStatus, sendCmd }) {
  return (
    <div className="panel">
      {/* Current motor state */}
      <div className="motor-status-card">
        <p className="motor-status-label">Motor Status</p>
        <p className="motor-status-val">{motorStatus}</p>
      </div>

      {/* Primary controls */}
      <div className="motor-btns">
        <button
          className="motor-btn motor-btn--contract"
          onClick={() => sendCmd('contract')}
          aria-label="Contract sleeve"
        >
          <span className="motor-btn-arrow">▲</span>
          Contract
        </button>
        <button
          className="motor-btn motor-btn--retract"
          onClick={() => sendCmd('retract')}
          aria-label="Retract sleeve"
        >
          <span className="motor-btn-arrow">▼</span>
          Retract
        </button>
      </div>

      <button
        className="motor-btn-stop"
        onClick={() => sendCmd('stop')}
        aria-label="Stop motor"
      >
        ◼ Stop
      </button>

      {/* Hardware reference */}
      <div className="pin-table">
        <p className="pin-table-title">Hardware Map</p>
        {PIN_ROWS.map(({ label, value }) => (
          <div key={label} className="pin-row">
            <span className="pin-label">{label}</span>
            <span className="pin-value">{value}</span>
          </div>
        ))}
      </div>

      <p className="hint">Manual commands pause auto-control for 10 s</p>
    </div>
  );
}
