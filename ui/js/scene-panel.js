// ── Scene Panel ───────────────────────────────────────────────────────────────
// Lists saved scenes, supports capture, recall, and morph-between.

import { send, on, state } from './app.js';

let selectedScene = null;
let morphing      = false;

export function initScenePanel() {
  const btnCapture = document.getElementById('btn-scene-capture');
  const btnRecall  = document.getElementById('btn-scene-recall');
  const btnMorph   = document.getElementById('btn-scene-morph');
  const morphSlider = document.getElementById('scene-morph-slider');

  if (btnCapture) {
    btnCapture.addEventListener('click', () => {
      const name = prompt('Scene name:', `Scene ${(state.sceneIds?.length ?? 0) + 1}`);
      if (name !== null) send('save_scene', { name });
    });
  }

  if (btnRecall) {
    btnRecall.addEventListener('click', () => {
      if (selectedScene != null) send('recall_scene', { scene_id: selectedScene });
    });
  }

  if (btnMorph) {
    btnMorph.addEventListener('click', () => {
      morphing = !morphing;
      btnMorph.classList.toggle('active', morphing);
      if (morphSlider) morphSlider.style.display = morphing ? '' : 'none';
    });
  }

  if (morphSlider) {
    morphSlider.style.display = 'none';
    morphSlider.addEventListener('input', e => {
      send('morph_scenes', { amount: parseFloat(e.target.value) / 100 });
    });
  }

  on('scenes_list', () => renderSceneList());
  on('tab_changed', tab => { if (tab === 'scenes') renderSceneList(); });

  renderSceneList();
}

// ── Scene list ────────────────────────────────────────────────────────────────

function renderSceneList() {
  const list = document.getElementById('scene-list');
  if (!list) return;

  list.innerHTML = '';
  const scenes = state.sceneIds ?? [];

  if (scenes.length === 0) {
    const empty = document.createElement('div');
    empty.className = 'scene-empty';
    empty.textContent = 'No scenes saved';
    list.appendChild(empty);
    return;
  }

  scenes.forEach(scene => {
    const id   = typeof scene === 'object' ? scene.scene_id ?? scene.id : scene;
    const name = typeof scene === 'object' ? scene.name ?? `Scene ${id}` : `Scene ${id}`;

    const row = document.createElement('div');
    row.className = 'scene-row' + (id === selectedScene ? ' selected' : '');
    row.dataset.sceneId = id;

    const label = document.createElement('span');
    label.className = 'scene-name';
    label.textContent = name;

    const btnDel = document.createElement('button');
    btnDel.className = 'scene-del-btn';
    btnDel.textContent = '✕';
    btnDel.title = 'Delete scene';
    btnDel.addEventListener('click', e => {
      e.stopPropagation();
      if (confirm(`Delete "${name}"?`)) {
        send('delete_scene', { scene_id: id });
        if (selectedScene === id) selectedScene = null;
      }
    });

    row.appendChild(label);
    row.appendChild(btnDel);
    row.addEventListener('click', () => {
      selectedScene = id;
      document.querySelectorAll('.scene-row').forEach(r =>
        r.classList.toggle('selected', r.dataset.sceneId == id));
    });
    row.addEventListener('dblclick', () => {
      send('recall_scene', { scene_id: id });
    });

    list.appendChild(row);
  });
}
