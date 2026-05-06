// ── LFO Editor ────────────────────────────────────────────────────────────────
// Left: LFO instance list. Center-left: 20 shape thumbnails.
// Center-right: large preview canvas (interactive for Custom shape).
// Right: depth / sync / phase controls.

import { send, on, state } from './app.js';

const SHAPE_NAMES = [
  'Sine','Triangle','Square 50%','Saw Up','Saw Down',
  'S & H','Smooth Rnd','Exp Rise','Exp Fall','Pulse 25%',
  'Pulse 75%','Stairs Up','Stairs Dn','Bounce','Sine²',
  'Trapezoid','Synco 1/8','Synco 1/16','Dotted Grvl','Shuffle 16',
  'Custom',
];

let selectedLfoId = null;
let selectedShape = 0;     // shape index 0–20
let customPoints  = [];    // [{phase, value}] for Custom shape
let draggingPt    = -1;    // index into customPoints during drag
let previewCanvas, previewCtx;

export function initLfoEditor() {
  previewCanvas = document.getElementById('lfo-preview-canvas');
  previewCtx    = previewCanvas.getContext('2d');

  buildShapeGrid();

  document.getElementById('btn-add-lfo').addEventListener('click', addLfo);
  document.getElementById('btn-apply-lfo').addEventListener('click', applyLfo);
  document.getElementById('btn-remove-lfo').addEventListener('click', removeLfo);

  document.getElementById('lfo-depth').addEventListener('input', e => {
    document.getElementById('lfo-depth-val').textContent = parseFloat(e.target.value).toFixed(2);
    requestPreview();
  });
  document.getElementById('lfo-phase').addEventListener('input', e => {
    document.getElementById('lfo-phase-val').textContent = parseFloat(e.target.value).toFixed(2);
    requestPreview();
  });
  document.getElementById('lfo-sync-div').addEventListener('change', requestPreview);
  document.getElementById('lfo-bipolar').addEventListener('change', requestPreview);

  // Custom canvas interactions.
  previewCanvas.addEventListener('mousedown', onPreviewMouseDown);
  previewCanvas.addEventListener('mousemove', onPreviewMouseMove);
  previewCanvas.addEventListener('mouseup',   onPreviewMouseUp);
  previewCanvas.addEventListener('contextmenu', onPreviewRightClick);

  on('lfos_list',    () => renderLfoList());
  on('lfo_preview',  msg => drawPreview(msg.values));
  on('tab_changed',  tab => { if (tab === 'lfo') { resizePreview(); requestPreview(); } });

  new ResizeObserver(() => { resizePreview(); requestPreview(); })
      .observe(previewCanvas.parentElement);

  resizePreview();
}

// ── Shape thumbnail grid ──────────────────────────────────────────────────────

function buildShapeGrid() {
  const grid = document.getElementById('shape-grid');
  grid.innerHTML = '';

  SHAPE_NAMES.forEach((name, i) => {
    const div = document.createElement('div');
    div.className = 'shape-thumb' + (i === selectedShape ? ' active' : '');
    div.dataset.shape = i;

    const c = document.createElement('canvas');
    c.width  = 72;
    c.height = 32;
    drawShapeThumb(c.getContext('2d'), c.width, c.height, i);

    const label = document.createElement('div');
    label.className   = 'shape-name';
    label.textContent = name;

    div.appendChild(c);
    div.appendChild(label);
    div.addEventListener('click', () => selectShape(i));
    grid.appendChild(div);
  });
}

function drawShapeThumb(ctx, W, H, shapeIdx) {
  ctx.fillStyle = '#0d1117';
  ctx.fillRect(0, 0, W, H);

  const pts = syntheticPreview(shapeIdx, 64);
  ctx.beginPath();
  pts.forEach((v, i) => {
    const x = (i / (pts.length - 1)) * W;
    const y = (1 - (v + 1) / 2) * H;
    i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
  });
  ctx.strokeStyle = '#58a6ff';
  ctx.lineWidth   = 1.5;
  ctx.stroke();
}

function selectShape(idx) {
  selectedShape = idx;
  document.querySelectorAll('.shape-thumb').forEach((el, i) =>
    el.classList.toggle('active', i === idx));
  const isCustom = idx === 20;
  document.getElementById('custom-hint').style.display = isCustom ? '' : 'none';
  requestPreview();
}

// ── LFO instance list ─────────────────────────────────────────────────────────

function renderLfoList() {
  const body = document.getElementById('lfo-list-body');
  body.innerHTML = '';

  state.lfos.forEach(lfo => {
    const div = document.createElement('div');
    div.className = 'lfo-item' + (lfo.lfo_id === selectedLfoId ? ' selected' : '');

    const name = document.createElement('span');
    name.className   = 'lfo-item-name';
    name.textContent = lfo.name || lfo.lfo_id;

    const shape = document.createElement('span');
    shape.className   = 'lfo-item-shape';
    shape.textContent = SHAPE_NAMES[lfo.shape] ?? '?';

    div.appendChild(name);
    div.appendChild(shape);
    div.addEventListener('click', () => selectLfo(lfo));
    body.appendChild(div);
  });
}

function selectLfo(lfo) {
  selectedLfoId = lfo.lfo_id;
  selectedShape = lfo.shape ?? 0;
  customPoints  = lfo.custom_points?.map(p => ({ phase: p.phase, value: p.value })) ?? [];

  document.getElementById('lfo-depth').value     = lfo.depth ?? 1;
  document.getElementById('lfo-depth-val').textContent = (lfo.depth ?? 1).toFixed(2);
  document.getElementById('lfo-sync-div').value  = lfo.sync_division ?? 3840;
  document.getElementById('lfo-phase').value     = lfo.phase_offset  ?? 0;
  document.getElementById('lfo-phase-val').textContent = (lfo.phase_offset ?? 0).toFixed(2);
  document.getElementById('lfo-bipolar').checked = lfo.bipolar ?? true;

  selectShape(selectedShape);
  renderLfoList();
  requestPreview();
}

function addLfo() {
  const id = 'lfo_' + Date.now();
  send('add_lfo', {
    lfo_id: id, name: `LFO ${state.lfos.length + 1}`,
    shape: 0, sync_to_tempo: true, sync_division: 3840,
    depth: 1, phase_offset: 0, bipolar: true,
  });
}

function applyLfo() {
  if (!selectedLfoId) { addLfo(); return; }
  const pts = customPoints.map(p => ({ phase: p.phase, value: p.value }));
  send('update_lfo', {
    lfo_id:        selectedLfoId,
    shape:         selectedShape,
    sync_to_tempo: true,
    sync_division: parseInt(document.getElementById('lfo-sync-div').value),
    depth:         parseFloat(document.getElementById('lfo-depth').value),
    phase_offset:  parseFloat(document.getElementById('lfo-phase').value),
    bipolar:       document.getElementById('lfo-bipolar').checked,
    custom_points: pts,
  });
}

function removeLfo() {
  if (!selectedLfoId) return;
  send('remove_lfo', { lfo_id: selectedLfoId });
  selectedLfoId = null;
}

// ── Preview canvas ────────────────────────────────────────────────────────────

function resizePreview() {
  const el = previewCanvas.parentElement;
  previewCanvas.width  = el.clientWidth  || 400;
  previewCanvas.height = el.clientHeight || 200;
}

function requestPreview() {
  if (selectedShape === 20) {
    // Draw custom shape from points locally (no server round-trip).
    drawCustomPreview();
    return;
  }
  send('get_lfo_preview', {
    resolution: 512,
    config: {
      shape:         selectedShape,
      sync_to_tempo: true,
      sync_division: parseInt(document.getElementById('lfo-sync-div').value),
      depth:         parseFloat(document.getElementById('lfo-depth').value),
      phase_offset:  parseFloat(document.getElementById('lfo-phase').value),
      bipolar:       document.getElementById('lfo-bipolar').checked,
    },
  });
}

function drawPreview(values) {
  if (!values?.length) return;
  const W = previewCanvas.width;
  const H = previewCanvas.height;
  const ctx = previewCtx;

  ctx.fillStyle = '#0d1117';
  ctx.fillRect(0, 0, W, H);

  // Grid.
  ctx.strokeStyle = '#1e2733';
  ctx.lineWidth   = 1;
  // Centre line.
  ctx.beginPath();
  ctx.moveTo(0, H / 2); ctx.lineTo(W, H / 2);
  ctx.stroke();
  // Quarter lines.
  [0.25, 0.5, 0.75].forEach(f => {
    ctx.beginPath();
    ctx.moveTo(f * W, 0); ctx.lineTo(f * W, H);
    ctx.stroke();
  });

  // Waveform fill.
  const midY = H / 2;
  ctx.fillStyle = '#58a6ff22';
  ctx.beginPath();
  values.forEach((v, i) => {
    const x = (i / (values.length - 1)) * W;
    const y = (1 - (v + 1) / 2) * H;
    i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
  });
  ctx.lineTo(W, midY); ctx.lineTo(0, midY);
  ctx.closePath();
  ctx.fill();

  // Waveform line.
  ctx.beginPath();
  values.forEach((v, i) => {
    const x = (i / (values.length - 1)) * W;
    const y = (1 - (v + 1) / 2) * H;
    i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
  });
  ctx.strokeStyle = '#58a6ff';
  ctx.lineWidth   = 2;
  ctx.stroke();
}

function drawCustomPreview() {
  const W   = previewCanvas.width;
  const H   = previewCanvas.height;
  const ctx = previewCtx;

  ctx.fillStyle = '#0d1117';
  ctx.fillRect(0, 0, W, H);

  // Grid lines.
  ctx.strokeStyle = '#1e2733';
  ctx.lineWidth   = 1;
  ctx.beginPath();
  ctx.moveTo(0, H / 2); ctx.lineTo(W, H / 2);
  ctx.stroke();
  [0.25,0.5,0.75].forEach(f => {
    ctx.beginPath(); ctx.moveTo(f*W, 0); ctx.lineTo(f*W, H); ctx.stroke();
  });

  if (customPoints.length < 2) {
    ctx.fillStyle = '#6e7681';
    ctx.font      = '12px monospace';
    ctx.textAlign = 'center';
    ctx.fillText('Click to add control points', W/2, H/2 - 10);
    ctx.textAlign = 'left';
    return;
  }

  // Sort by phase.
  const pts = [...customPoints].sort((a, b) => a.phase - b.phase);

  ctx.fillStyle   = '#58a6ff22';
  ctx.beginPath();
  pts.forEach((p, i) => {
    const x = p.phase * W;
    const y = (1 - (p.value + 1) / 2) * H;
    i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
  });
  ctx.lineTo(W, H/2); ctx.lineTo(0, H/2);
  ctx.closePath(); ctx.fill();

  ctx.beginPath();
  pts.forEach((p, i) => {
    const x = p.phase * W;
    const y = (1 - (p.value + 1) / 2) * H;
    i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
  });
  ctx.strokeStyle = '#58a6ff'; ctx.lineWidth = 2; ctx.stroke();

  // Control points.
  customPoints.forEach((p, i) => {
    const x = p.phase * W;
    const y = (1 - (p.value + 1) / 2) * H;
    ctx.beginPath();
    ctx.arc(x, y, 6, 0, Math.PI * 2);
    ctx.fillStyle   = i === draggingPt ? '#f78166' : '#e6edf3';
    ctx.strokeStyle = '#58a6ff';
    ctx.lineWidth   = 2;
    ctx.fill(); ctx.stroke();
  });
}

// ── Custom shape interaction ───────────────────────────────────────────────────

function ptFromEvent(e) {
  const rect = previewCanvas.getBoundingClientRect();
  return {
    phase: Math.max(0, Math.min(1, (e.clientX - rect.left)  / previewCanvas.width)),
    value: Math.max(-1, Math.min(1, 1 - ((e.clientY - rect.top) / previewCanvas.height) * 2)),
  };
}

function findNearestPoint(phase, value, threshPx) {
  const W = previewCanvas.width, H = previewCanvas.height;
  let best = -1, bestD = Infinity;
  customPoints.forEach((p, i) => {
    const dx = (p.phase - phase) * W;
    const dy = ((p.value - value) / 2) * H;
    const d  = Math.sqrt(dx*dx + dy*dy);
    if (d < threshD && d < bestD) { best = i; bestD = d; }
  });
  const threshD = threshPx;
  return best;
}

function onPreviewMouseDown(e) {
  if (selectedShape !== 20) return;
  const pt = ptFromEvent(e);
  const W  = previewCanvas.width, H = previewCanvas.height;
  // Find existing point within 12px.
  let best = -1, bestD = Infinity;
  customPoints.forEach((p, i) => {
    const dx = (p.phase - pt.phase) * W;
    const dy = ((p.value - pt.value) / 2) * H;
    const d  = Math.hypot(dx, dy);
    if (d < 12 && d < bestD) { best = i; bestD = d; }
  });
  if (best >= 0) {
    draggingPt = best;
  } else {
    customPoints.push(pt);
    draggingPt = customPoints.length - 1;
  }
  drawCustomPreview();
}

function onPreviewMouseMove(e) {
  if (selectedShape !== 20 || draggingPt < 0) return;
  const pt = ptFromEvent(e);
  customPoints[draggingPt] = pt;
  drawCustomPreview();
}

function onPreviewMouseUp() {
  draggingPt = -1;
}

function onPreviewRightClick(e) {
  if (selectedShape !== 20) return;
  e.preventDefault();
  const pt = ptFromEvent(e);
  const W  = previewCanvas.width, H = previewCanvas.height;
  let best = -1, bestD = Infinity;
  customPoints.forEach((p, i) => {
    const dx = (p.phase - pt.phase) * W;
    const dy = ((p.value - pt.value) / 2) * H;
    const d  = Math.hypot(dx, dy);
    if (d < 14 && d < bestD) { best = i; bestD = d; }
  });
  if (best >= 0) {
    customPoints.splice(best, 1);
    drawCustomPreview();
  }
}

// ── Offline shape approximation (for thumbnails, no server needed) ────────────

function syntheticPreview(shapeIdx, n) {
  const pts = [];
  for (let i = 0; i < n; i++) {
    const p = i / (n - 1);
    pts.push(evaluateShape(shapeIdx, p));
  }
  return pts;
}

function evaluateShape(s, p) {
  const TAU = Math.PI * 2;
  switch (s) {
    case 0:  return Math.sin(TAU * p);
    case 1:  return p < 0.5 ? 4*p-1 : 3-4*p;
    case 2:  return p < 0.5 ? 1 : -1;
    case 3:  return 2*p - 1;
    case 4:  return 1 - 2*p;
    case 5:  return 0.5;  // S&H — flat placeholder
    case 6:  return p < 0.5 ? -0.5 + p : 0.5 - p;  // smooth rnd placeholder
    case 7:  return 2*(Math.exp(p)-1)/(Math.E-1)-1;
    case 8:  return 2*(Math.exp(1-p)-1)/(Math.E-1)-1;
    case 9:  return p < 0.25 ? 1 : -1;
    case 10: return p < 0.75 ? 1 : -1;
    case 11: { const step=Math.floor(p*8); return 2*(step/7)-1; }
    case 12: { const step=Math.floor(p*8); return 1-2*(step/7); }
    case 13: return 2*Math.abs(Math.sin(Math.PI*p))-1;
    case 14: { const sv=Math.sin(Math.PI*p); return 2*sv*sv-1; }
    case 15: {
      if (p<0.1) return -1+20*p;
      if (p<0.5) return 1;
      if (p<0.6) return 1-20*(p-0.5);
      return -1;
    }
    case 16: { // Synco 1/8
      const slot=Math.floor(p*8)%8, sub=p*8-Math.floor(p*8);
      return slot%2===1 ? rhythmSlot(sub) : -0.8;
    }
    case 17: { // Synco 1/16
      const pat=[1,0,0,1,0,0,1,0,0,0,1,0,1,0,0,0];
      const slot=Math.floor(p*16)%16, sub=p*16-Math.floor(p*16);
      return pat[slot] ? rhythmSlot(sub) : -0.8;
    }
    case 18: { // 3+3+2
      const pat=[1,0,0,1,0,0,1,0];
      const slot=Math.floor(p*8)%8, sub=p*8-Math.floor(p*8);
      return pat[slot] ? rhythmSlot(sub) : -0.5;
    }
    case 19: { // Shuffle
      const eighth=p*8, sub=eighth-Math.floor(eighth);
      return sub < 2/3 ? rhythmSlot(sub/(2/3)) : -0.8;
    }
    default: return 0;
  }
}

function rhythmSlot(sub) {
  if (sub < 0.07) return -0.8 + (1.8/0.07)*sub;
  if (sub > 0.88) return 1 - (1.8/0.12)*(sub-0.88);
  return 1;
}
