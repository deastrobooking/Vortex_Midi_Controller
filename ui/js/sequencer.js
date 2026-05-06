// ── Canvas Step Sequencer ─────────────────────────────────────────────────────
// 64-track × 64-step grid. Left pane = track headers, right = step cells.
// Click = toggle step. Right-click = open step detail modal.

import { send, on, state } from './app.js';

// ── Palette — one color per track (cycles every 16) ──────────────────────────
const TRACK_COLORS = [
  '#58a6ff','#f78166','#3fb950','#d2a8ff','#ffa657','#79c0ff','#ff7b72','#56d364',
  '#e3b341','#bc8cff','#ff9a6c','#7ee787','#f0883e','#a5d6ff','#ffa198','#b3f0a0',
];

const HEADER_W   = 0;   // header is in the aside, not on canvas
const STEP_H     = 30;
const MIN_STEP_W = 10;
const MAX_STEP_W = 40;
const LABEL_W    = 0;

let canvas, ctx;
let zoom        = 1.5;
let scrollTop   = 0;
let selectedTrack = 0;
let stepModal   = null;
let modalTarget = null;   // {trackId, stepIdx}

export function initSequencer() {
  canvas   = document.getElementById('seq-canvas');
  ctx      = canvas.getContext('2d');
  stepModal = document.getElementById('step-modal');

  // Track list sidebar.
  buildTrackList();

  // Zoom control.
  document.getElementById('seq-zoom').addEventListener('input', e => {
    zoom = parseFloat(e.target.value);
    render();
  });

  // Canvas mouse events.
  canvas.addEventListener('click',       onCanvasClick);
  canvas.addEventListener('contextmenu', onCanvasRightClick);

  // Modal.
  document.getElementById('btn-sm-apply').addEventListener('click', applyStepModal);
  document.getElementById('btn-sm-cancel').addEventListener('click', () => stepModal.close());
  document.getElementById('sm-vel').addEventListener('input', e =>
    (document.getElementById('sm-vel-val').textContent = e.target.value));
  document.getElementById('sm-gate').addEventListener('input', e =>
    (document.getElementById('sm-gate-val').textContent = e.target.value));

  // State updates.
  on('sequencer_state', () => { buildTrackList(); render(); });

  // Resize observer.
  new ResizeObserver(resize).observe(canvas.parentElement);
  resize();

  // Animation loop for playhead.
  requestAnimationFrame(loop);
}

// ── Track list sidebar ────────────────────────────────────────────────────────

function buildTrackList() {
  const body = document.getElementById('track-list-body');
  body.innerHTML = '';
  const tracks = state.pattern.tracks ?? [];

  tracks.forEach((track, i) => {
    const row = document.createElement('div');
    row.className = 'track-row' + (i === selectedTrack ? ' selected' : '');
    row.dataset.trackId = i;

    const colorBar = document.createElement('div');
    colorBar.className = 'track-color-bar';
    colorBar.style.background = TRACK_COLORS[i % TRACK_COLORS.length];

    const name = document.createElement('div');
    name.className = 'track-name';
    name.textContent = track.name || `Track ${i + 1}`;

    const muteBtn = document.createElement('button');
    muteBtn.className = 'track-mute-btn' + (track.muted ? ' active' : '');
    muteBtn.textContent = 'M';
    muteBtn.title = 'Mute';
    muteBtn.addEventListener('click', e => {
      e.stopPropagation();
      send('mute_track', { track_id: i, muted: !track.muted });
    });

    const soloBtn = document.createElement('button');
    soloBtn.className = 'track-solo-btn';
    soloBtn.textContent = 'S';
    soloBtn.title = 'Solo';
    soloBtn.addEventListener('click', e => {
      e.stopPropagation();
      send('solo_track', { track_id: i, solo: true });
    });

    row.appendChild(colorBar);
    row.appendChild(name);
    row.appendChild(muteBtn);
    row.appendChild(soloBtn);
    row.addEventListener('click', () => selectTrack(i));
    body.appendChild(row);
  });
}

function selectTrack(id) {
  selectedTrack = id;
  document.querySelectorAll('.track-row').forEach((r, i) =>
    r.classList.toggle('selected', i === id));
  render();
}

// ── Canvas sizing ─────────────────────────────────────────────────────────────

function resize() {
  const container = canvas.parentElement;
  canvas.width  = container.clientWidth;
  canvas.height = Math.max(container.clientHeight,
                           (state.pattern.tracks?.length ?? 64) * STEP_H);
  render();
}

function stepWidth() {
  return Math.min(MAX_STEP_W, Math.max(MIN_STEP_W, 20 * zoom));
}

// ── Render ────────────────────────────────────────────────────────────────────

function render() {
  if (!ctx) return;
  const W = canvas.width;
  const sw = stepWidth();
  const tracks = state.pattern.tracks ?? [];

  ctx.fillStyle = '#0d1117';
  ctx.fillRect(0, 0, W, canvas.height);

  tracks.forEach((track, ti) => {
    const y     = ti * STEP_H;
    const color = TRACK_COLORS[ti % TRACK_COLORS.length];
    const muted = track.muted;
    const steps = track.steps ?? [];
    const count = track.stepCount ?? steps.length ?? 16;
    const curr  = state.currentSteps?.[ti] ?? 0;

    // Row background (alternate).
    if (ti % 2 === 0) {
      ctx.fillStyle = '#111820';
      ctx.fillRect(0, y, W, STEP_H);
    }

    // Selected track highlight.
    if (ti === selectedTrack) {
      ctx.fillStyle = '#1f3a5f22';
      ctx.fillRect(0, y, W, STEP_H);
    }

    // Step cells.
    for (let si = 0; si < count; ++si) {
      const x    = si * sw;
      const step = steps[si];

      // Playhead column.
      if (si === curr) {
        ctx.fillStyle = '#ffffff18';
        ctx.fillRect(x, y, sw, STEP_H);
      }

      // Bar dividers (every 16 steps).
      if (si % 16 === 0 && si > 0) {
        ctx.fillStyle = '#ffffff1a';
        ctx.fillRect(x, y, 1, STEP_H);
      }

      // Beat dividers (every 4 steps).
      if (si % 4 === 0 && si % 16 !== 0) {
        ctx.fillStyle = '#ffffff0a';
        ctx.fillRect(x, y, 1, STEP_H);
      }

      if (step?.active) {
        const vel   = (step.velocity ?? 100) / 127;
        const alpha = muted ? 0.3 : 0.85;
        ctx.globalAlpha = alpha;
        ctx.fillStyle   = muted ? '#555' : color;
        ctx.fillRect(x + 1, y + 4, sw - 2, STEP_H - 8);

        // Velocity bar.
        const vH = Math.round(vel * (STEP_H - 8));
        ctx.fillStyle = '#ffffff30';
        ctx.fillRect(x + 1, y + 4 + (STEP_H - 8 - vH), sw - 2, vH);

        // Accent dot.
        if (step.accent) {
          ctx.fillStyle = '#fff';
          ctx.beginPath();
          ctx.arc(x + sw / 2, y + 6, 2, 0, Math.PI * 2);
          ctx.fill();
        }

        // Slide indicator.
        if (step.slide) {
          ctx.fillStyle = '#fff8';
          ctx.fillRect(x + sw - 3, y + 4, 2, STEP_H - 8);
        }

        ctx.globalAlpha = 1;
      } else {
        // Empty cell border.
        ctx.strokeStyle = '#1e2733';
        ctx.strokeRect(x + 1.5, y + 4.5, sw - 3, STEP_H - 9);
      }
    }

    // Row separator.
    ctx.fillStyle = '#1e2733';
    ctx.fillRect(0, y + STEP_H - 1, W, 1);
  });
}

function loop() {
  render();
  requestAnimationFrame(loop);
}

// ── Interaction ───────────────────────────────────────────────────────────────

function hitTest(e) {
  const rect = canvas.getBoundingClientRect();
  const x    = e.clientX - rect.left;
  const y    = e.clientY - rect.top;
  const sw   = stepWidth();
  const ti   = Math.floor(y / STEP_H);
  const si   = Math.floor(x / sw);
  const tracks = state.pattern.tracks ?? [];
  if (ti < 0 || ti >= tracks.length) return null;
  const track = tracks[ti];
  if (si < 0 || si >= (track.stepCount ?? 16)) return null;
  return { trackId: ti, stepIdx: si, track, step: track.steps?.[si] };
}

function onCanvasClick(e) {
  const hit = hitTest(e);
  if (!hit) return;
  const { trackId, stepIdx, step } = hit;
  // Toggle active.
  send('set_step', {
    track_id:    trackId,
    step_idx:    stepIdx,
    active:      !(step?.active ?? false),
    note:        step?.note        ?? 60,
    velocity:    step?.velocity    ?? 100,
    gate_percent:step?.gatePercent ?? 75,
    accent:      step?.accent      ?? false,
    slide:       step?.slide       ?? false,
    pitch_offset:step?.pitchOffset ?? 0,
  });
  // Optimistic update.
  if (state.pattern.tracks?.[trackId]?.steps) {
    state.pattern.tracks[trackId].steps[stepIdx] ??= {};
    state.pattern.tracks[trackId].steps[stepIdx].active ^= true;
  }
}

function onCanvasRightClick(e) {
  e.preventDefault();
  const hit = hitTest(e);
  if (!hit) return;
  openStepModal(hit.trackId, hit.stepIdx, hit.step ?? {});
}

function openStepModal(trackId, stepIdx, step) {
  modalTarget = { trackId, stepIdx };
  document.getElementById('sm-note').value  = step.note        ?? 60;
  document.getElementById('sm-vel').value   = step.velocity    ?? 100;
  document.getElementById('sm-vel-val').textContent  = step.velocity ?? 100;
  document.getElementById('sm-gate').value  = step.gatePercent ?? 75;
  document.getElementById('sm-gate-val').textContent = step.gatePercent ?? 75;
  document.getElementById('sm-accent').checked = step.accent ?? false;
  document.getElementById('sm-slide').checked  = step.slide  ?? false;
  document.getElementById('sm-pitch-offset').value = step.pitchOffset ?? 0;
  stepModal.showModal();
}

function applyStepModal() {
  if (!modalTarget) return;
  const { trackId, stepIdx } = modalTarget;
  const step = state.pattern.tracks?.[trackId]?.steps?.[stepIdx] ?? {};
  send('set_step', {
    track_id:     trackId,
    step_idx:     stepIdx,
    active:       step.active ?? false,
    note:         parseInt(document.getElementById('sm-note').value),
    velocity:     parseInt(document.getElementById('sm-vel').value),
    gate_percent: parseInt(document.getElementById('sm-gate').value),
    accent:       document.getElementById('sm-accent').checked,
    slide:        document.getElementById('sm-slide').checked,
    pitch_offset: parseInt(document.getElementById('sm-pitch-offset').value),
  });
  stepModal.close();
}
