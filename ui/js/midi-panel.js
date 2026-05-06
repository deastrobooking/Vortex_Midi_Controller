// ── MIDI I/O Panel ────────────────────────────────────────────────────────────
// Lists discovered input ports, allows enable/disable per port.
// Outputs are always active (ALSA sequencer); shown read-only.

import { send, on, state } from './app.js';

let openPorts = new Set();   // portIds currently enabled

export function initMidiPanel() {
  const btnDiscover = document.getElementById('btn-midi-discover');
  if (btnDiscover) {
    btnDiscover.addEventListener('click', () => send('discover_midi_inputs'));
  }

  on('midi_inputs_list', () => renderPortList());
  on('tab_changed', tab => { if (tab === 'midi') renderPortList(); });
}

// ── Port list ─────────────────────────────────────────────────────────────────

function renderPortList() {
  const list = document.getElementById('midi-port-list');
  if (!list) return;

  list.innerHTML = '';
  const ports = state.midiInputPorts ?? [];

  if (ports.length === 0) {
    const empty = document.createElement('div');
    empty.className = 'midi-empty';
    empty.textContent = 'No MIDI inputs found — click Discover';
    list.appendChild(empty);
    return;
  }

  ports.forEach(port => {
    const id   = port.port_id ?? port.id ?? port;
    const name = port.name    ?? `Port ${id}`;
    const open = openPorts.has(id);

    const row = document.createElement('div');
    row.className = 'midi-port-row';

    const led = document.createElement('span');
    led.className = 'midi-port-led ' + (open ? 'open' : 'closed');

    const label = document.createElement('span');
    label.className = 'midi-port-name';
    label.textContent = name;

    const toggle = document.createElement('button');
    toggle.className = 'midi-port-toggle' + (open ? ' active' : '');
    toggle.textContent = open ? 'Enabled' : 'Enable';
    toggle.addEventListener('click', () => {
      if (openPorts.has(id)) {
        send('close_midi_input', { port_id: id });
        openPorts.delete(id);
      } else {
        send('open_midi_input', { port_id: id });
        openPorts.add(id);
      }
      renderPortList();
    });

    row.appendChild(led);
    row.appendChild(label);
    row.appendChild(toggle);
    list.appendChild(row);
  });
}
