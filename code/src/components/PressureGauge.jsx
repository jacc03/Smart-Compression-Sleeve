import {
  arcPath, valToAngle, getStatus, STATUS_COLOR,
  toRad, TARGET_MIN, TARGET_MAX, MAX_VAL,
} from '../utils/gaugeUtils';

const CX = 60;
const CY = 64;
const R  = 44;
const GAUGE_START = 135;
const GAUGE_END   = 405;

export default function PressureGauge({ value, pin }) {
  const vAngle  = valToAngle(value);
  const tMinAng = valToAngle(TARGET_MIN);
  const tMaxAng = valToAngle(TARGET_MAX);
  const status  = getStatus(value);
  const color   = STATUS_COLOR[status];

  // End-dot position
  const dotRad = toRad(vAngle);
  const dotX   = (CX + R * Math.cos(dotRad)).toFixed(2);
  const dotY   = (CY + R * Math.sin(dotRad)).toFixed(2);

  return (
    <div className="gauge-card">
      <svg viewBox="0 0 120 98" className="gauge-svg" aria-label={`Sensor on pin ${pin}: ${value.toFixed(1)} mmHg`}>
        {/* Background track */}
        <path
          d={arcPath(CX, CY, R, GAUGE_START, GAUGE_END)}
          fill="none" stroke="var(--border)" strokeWidth="8" strokeLinecap="round"
        />
        {/* Target zone highlight */}
        <path
          d={arcPath(CX, CY, R, tMinAng, tMaxAng)}
          fill="none" stroke="var(--accent)" strokeWidth="8"
          strokeLinecap="butt" strokeOpacity="0.28"
        />
        {/* Value arc */}
        {value > 0.3 && (
          <path
            d={arcPath(CX, CY, R, GAUGE_START, vAngle)}
            fill="none" stroke={color} strokeWidth="8" strokeLinecap="round"
          />
        )}
        {/* End dot */}
        {value > 0.3 && (
          <circle cx={dotX} cy={dotY} r="4.5" fill={color} />
        )}
        {/* Value readout */}
        <text x={CX} y={CY + 6}  textAnchor="middle" className="gauge-val"   fill={color}>
          {value.toFixed(1)}
        </text>
        <text x={CX} y={CY + 19} textAnchor="middle" className="gauge-unit"  fill="var(--text-2)">
          mmHg
        </text>
        {/* Scale labels */}
        <text x="16"  y="93" textAnchor="middle" className="gauge-scale" fill="var(--text-3)">0</text>
        <text x="104" y="93" textAnchor="middle" className="gauge-scale" fill="var(--text-3)">{MAX_VAL}</text>
      </svg>
      <div className="gauge-pin">pin {pin}</div>
    </div>
  );
}
