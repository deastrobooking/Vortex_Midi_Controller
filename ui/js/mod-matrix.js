// ── Modulation Matrix ─────────────────────────────────────────────────────────
// Table: rows = sources, columns = destinations.
// Cell click opens amount/curve popup. Active route shown with colored badge.

import { send, on, state } from './app.js';

// Ordered lists that define the grid axes.
// enum value matches ModSource / ModDest in Types.h (must stay in sync).
const SOURCES = [
  { id: 'lfo',         enum: 0, label: 'LFO'       },
  { id: 'velocity',    enum: 1, label: 'Velocity'  },
  { id: 'aftertouch',  enum: 2, label: 'Aftertouch'},
  { id: 'modwheel',    enum: 3, label: 'Mod Wheel' },
  { id: 'pitchbend',   enum: 4, label: 'Pitch Bend'},
  { id: 'midi_cc',     enum: 5, label: 'MIDI CC'   },
];

const DESTS = [
  { id: 'midi_cc',        enum: 0, label: 'MIDI CC'     },
  { id: 'seq_pitch',      enum: 1, label: 'Seq Pitch'   },
  { id: 'seq_velocity',   enum: 2, label: 'Seq Velocity'},
  { id: 'seq_gate',       enum: 3, label: 'Seq Gate'    },
  { id: 'seq_rate',       enum: 4, label: 'Seq Rate'    },
  { id: 'lfo_rate',       enum: 5, label: 'LFO Rate'    },
  { id: 'lfo_depth',      enum: 6, label: 'LFO Depth'   },
  { id: 'snapshot_morph', enum: 7, label: 'Scene Morph' },
];

// Build source_id from popup fields for each source type.
function buildSourceId(sourceEnum, lfoId, ccNum) {
  if (sourceEnum === 0) return lfoId ?? '';          // Lfo
  if (sourceEnum === 5) return `cc${ccNum ?? 1}`;   // MidiCC
  return '';
}

// Build dest_id from popup fields for each dest type.
function buildDestId(destEnum, trackId, lfoId) {
  if (destEnum >= 1 && destEnum <= 4) return String(trackId ?? 0); // Seq*
  if (destEnum === 5 || destEnum === 6) return lfoId ?? '';         // LfoRate/Depth
  return '';  // MidiCC uses controlId separately; SnapshotMorph not yet wired
}

// Simple unique ID for new routes.
let _routeSeq = 0;
function newRouteId() { return `r${Date.now()}_${++_routeSeq}`; }

let popup       = null;
let popupRoute  = null;   // { source, dest } or existing route object

export function initModMatrix() {
  buildTable();
  buildPopup();

  on('mod_routes_list', () => refreshCells());
  on('lfos_list',       () => refreshSourceOptions());
  on('tab_changed', tab => { if (tab === 'mod-matrix') refreshCells(); });
}

// ── Table construction ────────────────────────────────────────────────────────

function buildTable() {
  const wrap  = document.getElementById('mod-matrix-wrap');
  if (!wrap) return;

  const table = document.createElement('table');
  table.id = 'mod-matrix-table';
  table.className = 'mod-matrix-table';

  // Header row.
  const thead = table.createTHead();
  const hRow  = thead.insertRow();
  const th0   = document.createElement('th');
  th0.textContent = 'Source \\ Dest';
  hRow.appendChild(th0);
  DESTS.forEach(d => {
    const th = document.createElement('th');
    th.textContent = d.label;
    hRow.appendChild(th);
  });

  // Body rows.
  const tbody = table.createTBody();
  SOURCES.forEach(src => {
    const row = tbody.insertRow();
    const th  = document.createElement('th');
    th.textContent = src.label;
    row.appendChild(th);

    DESTS.forEach(dst => {
      const td = row.insertCell();
      td.dataset.source = src.id;
      td.dataset.dest   = dst.id;
      td.className = 'mm-cell';
      td.addEventListener('click', () => openPopup(src.id, dst.id, td));
    });
  });

  wrap.appendChild(table);
}

// ── Cell refresh ──────────────────────────────────────────────────────────────

function refreshCells() {
  const routes = state.modRoutes ?? [];
  document.querySelectorAll('.mm-cell').forEach(td => {
    const srcDef = SOURCES.find(s => s.id === td.dataset.source);
    const dstDef = DESTS.find(d => d.id === td.dataset.dest);
    // Server returns source/dest as integer enum values.
    const route = routes.find(r => r.source === srcDef?.enum && r.dest === dstDef?.enum);
    td.innerHTML = '';
    td.classList.toggle('mm-active', !!route);
    if (route) {
      const badge = document.createElement('span');
      badge.className = 'mm-badge';
      const pct = Math.round((route.amount ?? 1) * 100);
      badge.textContent = `${pct}%`;
      td.appendChild(badge);
    }
  });
}

function refreshSourceOptions() {
  // LFO source rows might show per-LFO labels eventually.
}

// ── Popup ─────────────────────────────────────────────────────────────────────

function buildPopup() {
  popup = document.createElement('div');
  popup.id = 'mm-popup';
  popup.className = 'mm-popup hidden';
  popup.innerHTML = `
    <div class="mm-popup-header">
      <span id="mm-popup-title">Route</span>
      <button id="mm-popup-close" title="Close">×</button>
    </div>
    <label>Amount
      <input type="range" id="mm-amount" min="-1" max="1" step="0.01" value="1">
      <span id="mm-amount-val">100%</span>
    </label>
    <label>LFO Instance
      <select id="mm-lfo-id"></select>
    </label>
    <label>CC Number
      <input type="number" id="mm-cc-num" min="0" max="127" value="1">
    </label>
    <label>Track
      <input type="number" id="mm-track-id" min="0" max="63" value="0">
    </label>
    <div class="mm-popup-actions">
      <button id="mm-btn-apply">Apply</button>
      <button id="mm-btn-remove">Remove</button>
    </div>
  `;
  document.body.appendChild(popup);

  document.getElementById('mm-popup-close').addEventListener('click', closePopup);
  document.getElementById('mm-amount').addEventListener('input', e => {
    const v = parseFloat(e.target.value);
    document.getElementById('mm-amount-val').textContent = `${Math.round(v * 100)}%`;
  });
  document.getElementById('mm-btn-apply').addEventListener('click', applyRoute);
  document.getElementById('mm-btn-remove').addEventListener('click', removeRoute);

  document.addEventListener('click', e => {
    if (popup && !popup.classList.contains('hidden') &&
        !popup.contains(e.target) && !e.target.classList.contains('mm-cell')) {
      closePopup();
    }
  });
}

function openPopup(source, dest, cell) {
  const srcDef = SOURCES.find(s => s.id === source);
  const dstDef = DESTS.find(d => d.id === dest);
  const route  = (state.modRoutes ?? []).find(
    r => r.source === srcDef?.enum && r.dest === dstDef?.enum
  );
  popupRoute = route ? { ...route, source, dest } : { source, dest, amount: 1 };

  document.getElementById('mm-popup-title').textContent =
    `${SOURCES.find(s => s.id === source)?.label} → ${DESTS.find(d => d.id === dest)?.label}`;

  const amount = popupRoute.amount ?? 1;
  document.getElementById('mm-amount').value = amount;
  document.getElementById('mm-amount-val').textContent = `${Math.round(amount * 100)}%`;
  document.getElementById('mm-cc-num').value   = popupRoute.controlId ?? 1;
  document.getElementById('mm-track-id').value = popupRoute.trackId   ?? 0;

  // Populate LFO list.
  const sel = document.getElementById('mm-lfo-id');
  sel.innerHTML = '<option value="">—</option>';
  (state.lfos ?? []).forEach(lfo => {
    const opt = document.createElement('option');
    opt.value       = lfo.lfo_id;
    opt.textContent = lfo.name || lfo.lfo_id;
    sel.appendChild(opt);
  });
  sel.value = popupRoute.lfoId ?? '';

  // Show/hide relevant fields.
  const lfoRow   = document.getElementById('mm-lfo-id').closest('label');
  const ccRow    = document.getElementById('mm-cc-num').closest('label');
  const trackRow = document.getElementById('mm-track-id').closest('label');
  lfoRow.style.display   = source === 'lfo'     ? '' : 'none';
  ccRow.style.display    = source === 'midi_cc' || dest === 'midi_cc' ? '' : 'none';
  trackRow.style.display = dest.startsWith('seq_') ? '' : 'none';

  // Position popup near cell.
  const rect = cell.getBoundingClientRect();
  popup.style.top  = `${rect.bottom + window.scrollY + 4}px`;
  popup.style.left = `${Math.min(rect.left + window.scrollX, window.innerWidth - 260)}px`;
  popup.classList.remove('hidden');
}

function closePopup() {
  popup.classList.add('hidden');
  popupRoute = null;
}

function applyRoute() {
  if (!popupRoute) return;
  const lfoId   = document.getElementById('mm-lfo-id').value || '';
  const ccNum   = parseInt(document.getElementById('mm-cc-num').value) || 1;
  const trackId = parseInt(document.getElementById('mm-track-id').value) || 0;
  const amount  = parseFloat(document.getElementById('mm-amount').value);

  const srcDef = SOURCES.find(s => s.id === popupRoute.source);
  const dstDef = DESTS.find(d => d.id === popupRoute.dest);
  const srcEnum = srcDef?.enum ?? 0;
  const dstEnum = dstDef?.enum ?? 0;

  const existing = (state.modRoutes ?? []).find(
    r => r.route_id === popupRoute.route_id
  );

  const payload = {
    route_id:  existing?.route_id ?? newRouteId(),
    source:    srcEnum,
    source_id: buildSourceId(srcEnum, lfoId, ccNum),
    dest:      dstEnum,
    dest_id:   buildDestId(dstEnum, trackId, lfoId),
    amount,
    offset:    0,
    enabled:   true,
  };

  if (existing) {
    send('update_mod_route', payload);
  } else {
    send('add_mod_route', payload);
  }
  closePopup();
}

function removeRoute() {
  if (!popupRoute?.route_id) { closePopup(); return; }
  send('remove_mod_route', { route_id: popupRoute.route_id });
  closePopup();
}
