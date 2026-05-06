// ── Melodic Piano Roll ─────────────────────────────────────────────────────────
// Shows a selected track's steps as note cells on a pitch grid.
// Left: piano keyboard. Right: grid (x=step, y=pitch).
// Scale degrees highlighted. Click to set/clear a note.

import { send, on, state } from './app.js';

const NOTE_H      = 10;   // px per semitone row
const PIANO_W     = 48;   // piano keyboard width
const STEP_W      = 28;   // px per step
const MIN_NOTE    = 24;   // C1
const MAX_NOTE    = 96;   // C7
const NOTE_COUNT  = MAX_NOTE - MIN_NOTE + 1;

const NOTE_NAMES  = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
const IS_BLACK    = [0,1,0,1,0,0,1,0,1,0,1,0].map(Boolean);

let pianoCanvas, pianoCtx;
let rollCanvas,  rollCtx;
let activeTrackId = 0;
let activeScale   = null;   // Scale object or null
let baseOctave    = 4;

export function initPianoRoll() {
  pianoCanvas = document.getElementById('piano-canvas');
  pianoCtx    = pianoCanvas.getContext('2d');
  rollCanvas  = document.getElementById('pr-canvas');
  rollCtx     = rollCanvas.getContext('2d');

  // Size piano keyboard (fixed width, full pitch height).
  pianoCanvas.width  = PIANO_W;
  pianoCanvas.height = NOTE_COUNT * NOTE_H;
  drawPianoKeyboard();

  // Track selector.
  const trackSel = document.getElementById('pr-track-select');
  trackSel.addEventListener('change', () => {
    activeTrackId = parseInt(trackSel.value);
    renderRoll();
  });

  // Scale selector.
  const scaleSel = document.getElementById('pr-scale-select');
  scaleSel.addEventListener('change', () => {
    activeScale = state.scales.find(s => s.scale_id === scaleSel.value) ?? null;
    drawPianoKeyboard();
    renderRoll();
  });

  // Click on roll.
  rollCanvas.addEventListener('click', onRollClick);

  // State events.
  on('sequencer_state', () => {
    populateTrackSelect();
    resizeRoll();
    renderRoll();
  });
  on('scales_list', () => populateScaleSelect());
  on('tab_changed', tab => { if (tab === 'piano-roll') { resizeRoll(); renderRoll(); } });

  new ResizeObserver(resizeRoll).observe(rollCanvas.parentElement);
  resizeRoll();
}

function populateTrackSelect() {
  const sel = document.getElementById('pr-track-select');
  const val = sel.value;
  sel.innerHTML = '';
  (state.pattern.tracks ?? []).forEach((t, i) => {
    const opt = document.createElement('option');
    opt.value       = i;
    opt.textContent = t.name || `Track ${i + 1}`;
    sel.appendChild(opt);
  });
  sel.value = val || '0';
  activeTrackId = parseInt(sel.value);
}

function populateScaleSelect() {
  const sel = document.getElementById('pr-scale-select');
  sel.innerHTML = '<option value="">None</option>';
  state.scales.forEach(s => {
    const opt = document.createElement('option');
    opt.value       = s.scale_id;
    opt.textContent = s.name;
    sel.appendChild(opt);
  });
}

// ── Piano keyboard ────────────────────────────────────────────────────────────

function drawPianoKeyboard() {
  pianoCtx.fillStyle = '#0d1117';
  pianoCtx.fillRect(0, 0, PIANO_W, pianoCanvas.height);

  for (let n = MAX_NOTE; n >= MIN_NOTE; n--) {
    const y       = (MAX_NOTE - n) * NOTE_H;
    const noteClass = n % 12;
    const black   = IS_BLACK[noteClass];
    const inScale = activeScale ? (activeScale.degrees?.[noteClass]) : false;

    if (black) {
      pianoCtx.fillStyle = inScale ? '#3a5a9f' : '#1a1a2e';
    } else {
      pianoCtx.fillStyle = inScale ? '#6fa8f5' : '#c9d1d9';
    }
    pianoCtx.fillRect(0, y, black ? PIANO_W * 0.7 : PIANO_W, NOTE_H - 1);

    // Note name on C notes.
    if (noteClass === 0) {
      pianoCtx.fillStyle = black ? '#c9d1d9' : '#0d1117';
      pianoCtx.font      = '8px monospace';
      pianoCtx.fillText(`C${Math.floor(n / 12) - 1}`, 2, y + NOTE_H - 2);
    }
  }
}

// ── Roll canvas ───────────────────────────────────────────────────────────────

function resizeRoll() {
  const container = rollCanvas.parentElement;
  const track     = state.pattern.tracks?.[activeTrackId];
  const stepCount = track?.stepCount ?? 16;
  rollCanvas.width  = Math.max(container.clientWidth - PIANO_W, stepCount * STEP_W);
  rollCanvas.height = NOTE_COUNT * NOTE_H;
  renderRoll();
}

function renderRoll() {
  if (!rollCtx) return;
  const W     = rollCanvas.width;
  const track = state.pattern.tracks?.[activeTrackId];
  if (!track) return;

  const steps = track.steps ?? [];
  const count = track.stepCount ?? steps.length ?? 16;

  rollCtx.fillStyle = '#0d1117';
  rollCtx.fillRect(0, 0, W, rollCanvas.height);

  // Horizontal rows (pitch).
  for (let n = MAX_NOTE; n >= MIN_NOTE; n--) {
    const y         = (MAX_NOTE - n) * NOTE_H;
    const noteClass = n % 12;
    const black     = IS_BLACK[noteClass];
    const inScale   = activeScale ? (activeScale.degrees?.[noteClass]) : false;

    // Row background.
    if (black) {
      rollCtx.fillStyle = inScale ? '#0f1a2e' : '#0a0d14';
    } else {
      rollCtx.fillStyle = inScale ? '#141f35' : '#0d1117';
    }
    rollCtx.fillRect(0, y, W, NOTE_H);

    // Row separator.
    rollCtx.fillStyle = '#1e2733';
    rollCtx.fillRect(0, y + NOTE_H - 1, W, 1);
  }

  // Vertical step columns.
  for (let si = 0; si < count; si++) {
    const x = si * STEP_W;

    // Bar / beat dividers.
    if (si % 16 === 0) {
      rollCtx.fillStyle = '#ffffff22';
      rollCtx.fillRect(x, 0, 1, rollCanvas.height);
    } else if (si % 4 === 0) {
      rollCtx.fillStyle = '#ffffff0a';
      rollCtx.fillRect(x, 0, 1, rollCanvas.height);
    }

    const step = steps[si];
    if (!step?.active) continue;

    const note = Math.min(MAX_NOTE, Math.max(MIN_NOTE, step.note ?? 60));
    const y    = (MAX_NOTE - note) * NOTE_H;
    const vel  = (step.velocity ?? 100) / 127;

    rollCtx.fillStyle   = `hsla(${(note * 3) % 360}, 70%, 55%, 0.85)`;
    rollCtx.fillRect(x + 1, y + 1, STEP_W - 2, NOTE_H - 2);

    // Velocity shading.
    rollCtx.fillStyle = `rgba(255,255,255,${0.3 * (1 - vel)})`;
    rollCtx.fillRect(x + 1, y + 1, STEP_W - 2, NOTE_H - 2);
  }
}

// ── Interaction ───────────────────────────────────────────────────────────────

function onRollClick(e) {
  const rect     = rollCanvas.getBoundingClientRect();
  const x        = e.clientX - rect.left;
  const y        = e.clientY - rect.top;
  const stepIdx  = Math.floor(x / STEP_W);
  const noteFromTop = Math.floor(y / NOTE_H);
  const note     = MAX_NOTE - noteFromTop;

  if (note < MIN_NOTE || note > MAX_NOTE) return;

  const track = state.pattern.tracks?.[activeTrackId];
  if (!track) return;
  if (stepIdx >= (track.stepCount ?? 16)) return;

  const step    = track.steps?.[stepIdx] ?? {};
  const quant   = activeScale ? quantizeToScale(note, activeScale) : note;
  const toggling = step.active && step.note === quant;

  send('set_step', {
    track_id:     activeTrackId,
    step_idx:     stepIdx,
    active:       !toggling,
    note:         quant,
    velocity:     step.velocity    ?? 100,
    gate_percent: step.gatePercent ?? 75,
    accent:       step.accent      ?? false,
    slide:        step.slide       ?? false,
    pitch_offset: step.pitchOffset ?? 0,
  });

  // Optimistic update.
  if (state.pattern.tracks?.[activeTrackId]?.steps) {
    state.pattern.tracks[activeTrackId].steps[stepIdx] = {
      ...(step ?? {}),
      active: !toggling,
      note:   quant,
    };
  }
  renderRoll();
}

function quantizeToScale(note, scale) {
  if (!scale?.degrees) return note;
  if (scale.degrees[note % 12]) return note;
  for (let d = 1; d <= 12; d++) {
    if (note - d >= 0   && scale.degrees[(note - d) % 12]) return note - d;
    if (note + d <= 127 && scale.degrees[(note + d) % 12]) return note + d;
  }
  return note;
}
