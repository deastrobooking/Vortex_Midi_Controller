// ── Arrangement Timeline ──────────────────────────────────────────────────────
// Bar-based canvas timeline. Rows = tracks, columns = bars.
// Drag to place pattern clips; right-click to remove.

import { send, on, state } from './app.js';

const ROW_H    = 32;
const BAR_W    = 48;
const HEADER_H = 24;
const BARS     = 64;   // visible timeline length in bars

let canvas, ctx;
let clips    = [];    // { trackId, bar, length, patternId }
let dragging = null;  // { trackId, bar } while mouse held

export function initArrange() {
  canvas = document.getElementById('arrange-canvas');
  if (!canvas) return;
  ctx = canvas.getContext('2d');

  canvas.addEventListener('mousedown',   onMouseDown);
  canvas.addEventListener('mousemove',   onMouseMove);
  canvas.addEventListener('mouseup',     onMouseUp);
  canvas.addEventListener('contextmenu', onRightClick);

  new ResizeObserver(resize).observe(canvas.parentElement);
  resize();

  on('sequencer_state', () => render());
  on('tab_changed', tab => { if (tab === 'arrange') { resize(); render(); } });
}

// ── Canvas sizing ─────────────────────────────────────────────────────────────

function resize() {
  if (!canvas) return;
  const container = canvas.parentElement;
  const tracks = state.pattern.tracks?.length ?? 16;
  canvas.width  = Math.max(container.clientWidth, BARS * BAR_W);
  canvas.height = HEADER_H + tracks * ROW_H;
  render();
}

// ── Render ────────────────────────────────────────────────────────────────────

function render() {
  if (!ctx) return;
  const W = canvas.width;
  const H = canvas.height;
  const tracks = state.pattern.tracks ?? [];
  const currentBar = state.currentBar ?? 0;

  ctx.fillStyle = '#0d1117';
  ctx.fillRect(0, 0, W, H);

  // Bar ruler.
  ctx.fillStyle = '#161b22';
  ctx.fillRect(0, 0, W, HEADER_H);
  ctx.fillStyle = '#8b949e';
  ctx.font = '10px monospace';
  for (let b = 0; b < BARS; b++) {
    const x = b * BAR_W;
    if (b % 4 === 0) {
      ctx.fillStyle = '#30363d';
      ctx.fillRect(x, 0, 1, H);
      ctx.fillStyle = '#8b949e';
      ctx.fillText(b + 1, x + 3, HEADER_H - 5);
    } else {
      ctx.fillStyle = '#21262d';
      ctx.fillRect(x, HEADER_H, 1, H - HEADER_H);
    }
  }

  // Playhead.
  const phX = currentBar * BAR_W;
  ctx.fillStyle = '#58a6ff44';
  ctx.fillRect(phX, 0, BAR_W, H);
  ctx.fillStyle = '#58a6ff';
  ctx.fillRect(phX, 0, 2, H);

  // Track rows.
  tracks.forEach((track, ti) => {
    const y = HEADER_H + ti * ROW_H;

    if (ti % 2 === 0) {
      ctx.fillStyle = '#111820';
      ctx.fillRect(0, y, W, ROW_H);
    }

    // Row separator.
    ctx.fillStyle = '#1e2733';
    ctx.fillRect(0, y + ROW_H - 1, W, 1);
  });

  // Pattern clips.
  clips.forEach(clip => {
    const track = tracks[clip.trackId];
    if (!track) return;
    const y   = HEADER_H + clip.trackId * ROW_H;
    const x   = clip.bar * BAR_W;
    const w   = (clip.length ?? 1) * BAR_W - 2;
    const col = `hsl(${(clip.trackId * 37) % 360}, 55%, 40%)`;

    ctx.fillStyle = col;
    ctx.fillRect(x + 1, y + 3, w, ROW_H - 6);

    ctx.fillStyle = '#ffffff99';
    ctx.font = '10px monospace';
    ctx.fillText(track.name || `T${clip.trackId + 1}`, x + 4, y + ROW_H - 9);
  });
}

// ── Interaction ───────────────────────────────────────────────────────────────

function hitTest(e) {
  const rect = canvas.getBoundingClientRect();
  const x    = e.clientX - rect.left;
  const y    = e.clientY - rect.top - HEADER_H;
  if (y < 0) return null;
  const trackId = Math.floor(y / ROW_H);
  const bar     = Math.floor(x / BAR_W);
  const tracks  = state.pattern.tracks ?? [];
  if (trackId < 0 || trackId >= tracks.length) return null;
  if (bar < 0 || bar >= BARS) return null;
  return { trackId, bar };
}

function onMouseDown(e) {
  if (e.button !== 0) return;
  const hit = hitTest(e);
  if (!hit) return;
  dragging = hit;

  const existing = clips.findIndex(c => c.trackId === hit.trackId && c.bar === hit.bar);
  if (existing === -1) {
    clips.push({ trackId: hit.trackId, bar: hit.bar, length: 1 });
    send('arrange_set_clip', { track_id: hit.trackId, bar: hit.bar, length: 1 });
  }
  render();
}

function onMouseMove(e) {
  if (!dragging) return;
  const rect = canvas.getBoundingClientRect();
  const x    = e.clientX - rect.left;
  const endBar = Math.max(dragging.bar + 1, Math.floor(x / BAR_W) + 1);
  const idx = clips.findIndex(c => c.trackId === dragging.trackId && c.bar === dragging.bar);
  if (idx !== -1) {
    clips[idx].length = endBar - dragging.bar;
    render();
  }
}

function onMouseUp(e) {
  if (!dragging) return;
  const idx = clips.findIndex(c => c.trackId === dragging.trackId && c.bar === dragging.bar);
  if (idx !== -1) {
    const clip = clips[idx];
    send('arrange_set_clip', { track_id: clip.trackId, bar: clip.bar, length: clip.length });
  }
  dragging = null;
}

function onRightClick(e) {
  e.preventDefault();
  const hit = hitTest(e);
  if (!hit) return;
  const idx = clips.findIndex(c => c.trackId === hit.trackId && c.bar === hit.bar);
  if (idx !== -1) {
    clips.splice(idx, 1);
    send('arrange_remove_clip', { track_id: hit.trackId, bar: hit.bar });
    render();
  }
}
