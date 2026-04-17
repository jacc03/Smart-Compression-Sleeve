export const TARGET_MIN = 20;
export const TARGET_MAX = 40;
export const MAX_VAL    = 50;
export const FSR_PINS   = [34, 35, 32, 33];

export const toRad = (deg) => (deg * Math.PI) / 180;

export const polarToCart = (cx, cy, r, deg) => ({
  x: cx + r * Math.cos(toRad(deg)),
  y: cy + r * Math.sin(toRad(deg)),
});

/**
 * Returns an SVG arc path string.
 * Handles angles > 360 (e.g. the full track ends at 135+270=405).
 */
export const arcPath = (cx, cy, r, startDeg, endDeg) => {
  if (endDeg - startDeg < 0.5) return '';
  const s = polarToCart(cx, cy, r, startDeg);
  const e = polarToCart(cx, cy, r, endDeg % 360);
  const large = endDeg - startDeg > 180 ? 1 : 0;
  return `M${s.x.toFixed(2)},${s.y.toFixed(2)} A${r},${r} 0 ${large} 1 ${e.x.toFixed(2)},${e.y.toFixed(2)}`;
};

/** Maps a pressure value (0–MAX_VAL) to an arc angle (135°–405°). */
export const valToAngle = (v) =>
  135 + (Math.min(Math.max(v, 0), MAX_VAL) / MAX_VAL) * 270;

export const getStatus = (v) =>
  v < TARGET_MIN ? 'low' : v > TARGET_MAX ? 'high' : 'ok';

export const STATUS_COLOR = {
  ok:   'var(--ok)',
  low:  'var(--high)', // below target → red (needs tightening)
  high: 'var(--low)',  // above target → blue (needs releasing)
};
