// ── Vortex UI — Main Application Module ───────────────────────────────────────
// WebSocket connection, central state, event bus, message dispatch.

import { initTransport }  from './transport.js';
import { initSequencer }  from './sequencer.js';
import { initPianoRoll }  from './piano-roll.js';
import { initLfoEditor }  from './lfo-editor.js';
import { initModMatrix }  from './mod-matrix.js';
import { initScenePanel } from './scene-panel.js';
import { initMidiPanel }  from './midi-panel.js';
import { initArrange }    from './arrange.js';

// ── Central state ─────────────────────────────────────────────────────────────

export const state = {
  connected:   false,
  playing:     false,
  tempo:       120.0,
  currentTick: 0,
  currentBeat: 0,
  currentBar:  0,
  sceneIds:    [],
  pattern:     { patternId: '', name: '', lengthBars: 1, tracks: [] },
  currentSteps: new Array(64).fill(0),
  lfos:        [],
  modRoutes:   [],
  scales:      [],
  midiInputPorts: [],
  patternIds:  [],
  arrangeClips: [],
  arpConfigs:  {},   // trackId → ArpConfig
};

// ── Event bus ─────────────────────────────────────────────────────────────────

const listeners = {};

export function on(event, fn) {
  (listeners[event] ??= []).push(fn);
}

export function emit(event, data) {
  (listeners[event] ?? []).forEach(fn => fn(data));
}

// ── WebSocket ─────────────────────────────────────────────────────────────────

let ws        = null;
let reconnect = null;

export function send(type, payload = {}) {
  if (ws?.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ type, ...payload }));
  }
}

function connect() {
  const url = `ws://${location.hostname || 'localhost'}:8080`;
  ws = new WebSocket(url);

  ws.addEventListener('open', () => {
    state.connected = true;
    document.getElementById('conn-dot').className   = 'dot connected';
    document.getElementById('conn-label').textContent = 'Connected';
    if (reconnect) { clearInterval(reconnect); reconnect = null; }

    // Initial state fetch.
    send('get_project_state');
    send('get_sequencer_state');
    send('list_lfos');
    send('list_mod_routes');
    send('list_scales');
    send('list_scenes');
    send('list_patterns', { project_id: 1 });
    send('arrange_get_clips');
  });

  ws.addEventListener('close', () => {
    state.connected = false;
    document.getElementById('conn-dot').className    = 'dot disconnected';
    document.getElementById('conn-label').textContent = 'Disconnected';
    reconnect ??= setInterval(connect, 3000);
  });

  ws.addEventListener('error', () => ws.close());

  ws.addEventListener('message', ({ data }) => {
    let msg;
    try { msg = JSON.parse(data); } catch { return; }
    dispatch(msg);
  });
}

// ── Message dispatch ──────────────────────────────────────────────────────────

function dispatch(msg) {
  switch (msg.type) {

    case 'project_state':
      state.tempo        = msg.tempo;
      state.playing      = msg.playing;
      state.currentTick  = msg.current_tick;
      state.currentBeat  = msg.current_beat;
      state.currentBar   = msg.current_bar;
      state.sceneIds     = msg.scene_ids ?? [];
      emit('project_state', msg);
      blinkMidiOut();
      break;

    case 'sequencer_state':
      state.pattern      = msg;          // entire response reused as pattern obj
      state.currentSteps = msg.current_steps ?? new Array(64).fill(0);
      emit('sequencer_state', msg);
      break;

    case 'control_values':
      emit('control_values', msg.values);
      blinkMidiOut();
      break;

    case 'lfos_list':
      state.lfos = msg.lfos ?? [];
      emit('lfos_list', state.lfos);
      break;

    case 'mod_routes_list':
      state.modRoutes = msg.routes ?? [];
      emit('mod_routes_list', state.modRoutes);
      break;

    case 'scales_list':
      state.scales = msg.scales ?? [];
      emit('scales_list', state.scales);
      break;

    case 'scenes_list':
      state.sceneIds = msg.scenes ?? [];
      emit('scenes_list', state.sceneIds);
      break;

    case 'patterns_list':
      state.patternIds = msg.pattern_ids ?? [];
      emit('patterns_list', state.patternIds);
      break;

    case 'pattern_loaded':
      send('get_sequencer_state');
      break;

    case 'lfo_preview':
      emit('lfo_preview', msg);
      break;

    case 'midi_inputs_list':
      state.midiInputPorts = msg.ports ?? [];
      emit('midi_inputs_list', state.midiInputPorts);
      break;

    case 'lfo_added':
    case 'lfo_updated':
    case 'lfo_removed':
      send('list_lfos');
      break;

    case 'mod_route_added':
    case 'mod_route_updated':
    case 'mod_route_removed':
      send('list_mod_routes');
      break;

    case 'scene_saved':
    case 'scene_deleted':
      send('list_scenes');
      break;

    case 'pattern_saved':
      send('list_patterns', { project_id: 1 });
      break;

    case 'transport_ack':
      send('get_project_state');
      break;

    case 'tempo_set':
      state.tempo = msg.tempo;
      emit('tempo_changed', msg.tempo);
      break;

    case 'arp_config':
      state.arpConfigs[msg.track_id] = msg;
      emit('arp_config', msg);
      break;

    case 'arrange_clips':
      state.arrangeClips = msg.clips ?? [];
      emit('arrange_clips', state.arrangeClips);
      break;

    // Acknowledge-only responses — no state update needed.
    case 'arp_set':
    case 'arrange_clip_set':
    case 'arrange_clip_removed':
    case 'scene_triggered':
    case 'mapping_updated':
    case 'record_started':
    case 'record_stopped':
      break;

    default:
      break;
  }
}

// ── MIDI activity LEDs ────────────────────────────────────────────────────────

let midiOutTimer = null;
function blinkMidiOut() {
  const led = document.getElementById('led-midi-out');
  led.classList.add('blink');
  clearTimeout(midiOutTimer);
  midiOutTimer = setTimeout(() => led.classList.remove('blink'), 80);
}

// ── Tab switching ─────────────────────────────────────────────────────────────

function initTabs() {
  document.querySelectorAll('#tab-bar .tab').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('#tab-bar .tab').forEach(b => b.classList.remove('active'));
      document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
      btn.classList.add('active');
      const target = document.getElementById(`tab-${btn.dataset.tab}`);
      if (target) target.classList.add('active');
      emit('tab_changed', btn.dataset.tab);
    });
  });
}

// ── Boot ──────────────────────────────────────────────────────────────────────

document.addEventListener('DOMContentLoaded', () => {
  initTabs();
  initTransport();
  initSequencer();
  initPianoRoll();
  initLfoEditor();
  initModMatrix();
  initScenePanel();
  initMidiPanel();
  initArrange();
  connect();
});
