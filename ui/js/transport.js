// ── Transport Bar ──────────────────────────────────────────────────────────────

import { send, on, state } from './app.js';

export function initTransport() {
  const bpmInput  = document.getElementById('bpm-input');
  const btnPlay   = document.getElementById('btn-play');
  const btnStop   = document.getElementById('btn-stop');
  const btnTap    = document.getElementById('btn-tap');
  const posBar    = document.getElementById('pos-bar');
  const posBeat   = document.getElementById('pos-beat');
  const posTick   = document.getElementById('pos-tick');

  btnPlay.addEventListener('click', () => {
    send('transport', { action: state.playing ? 'continue' : 'play' });
  });

  btnStop.addEventListener('click', () => {
    send('transport', { action: 'stop' });
  });

  btnTap.addEventListener('click', () => {
    send('transport', { action: 'tap' });
  });

  bpmInput.addEventListener('change', () => {
    const bpm = parseFloat(bpmInput.value);
    if (!isNaN(bpm)) send('set_tempo', { tempo: bpm });
  });
  bpmInput.addEventListener('keydown', e => {
    if (e.key === 'Enter') bpmInput.blur();
    if (e.key === 'ArrowUp')   { e.preventDefault(); nudgeBpm(+0.1); }
    if (e.key === 'ArrowDown') { e.preventDefault(); nudgeBpm(-0.1); }
  });

  function nudgeBpm(delta) {
    const bpm = parseFloat(bpmInput.value) + delta;
    bpmInput.value = bpm.toFixed(1);
    send('set_tempo', { tempo: bpm });
  }

  on('project_state', msg => {
    bpmInput.value = msg.tempo.toFixed(1);
    btnPlay.classList.toggle('active', msg.playing);
    btnStop.classList.toggle('active', !msg.playing);

    const bar  = msg.current_bar  + 1;
    const beat = (msg.current_beat % 4) + 1;
    const tick = msg.current_tick % 960;
    posBar.textContent  = bar;
    posBeat.textContent = beat;
    posTick.textContent = String(tick).padStart(3, '0');
  });

  on('tempo_changed', bpm => {
    bpmInput.value = bpm.toFixed(1);
  });
}
