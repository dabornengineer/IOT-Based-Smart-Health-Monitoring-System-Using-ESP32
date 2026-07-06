'use strict';

// ── ECG canvas ────────────────────────────────────────────────────────────────
const canvas = document.getElementById('ecgCanvas');
const ctx    = canvas.getContext('2d');
let ecgData  = new Array(200).fill(2048);

function resizeCanvas() { canvas.width = canvas.offsetWidth; canvas.height = canvas.offsetHeight; }
resizeCanvas();
window.addEventListener('resize', resizeCanvas);

function drawEcg(data, leadsOff) {
  const W = canvas.width, H = canvas.height;
  ctx.clearRect(0, 0, W, H);
  if (leadsOff) {
    ctx.fillStyle = '#94a3b8';
    ctx.font = '13px system-ui, sans-serif';
    ctx.textAlign = 'center';
    ctx.fillText('— LEADS OFF —', W / 2, H / 2);
    return;
  }
  let lo = Infinity, hi = -Infinity;
  for (let i = 0; i < data.length; i++) { if (data[i] < lo) lo = data[i]; if (data[i] > hi) hi = data[i]; }
  const range = hi - lo || 1, pad = H * 0.1;
  ctx.beginPath();
  ctx.strokeStyle = '#2563eb';
  ctx.lineWidth   = 2;
  ctx.shadowColor = '#2563eb';
  ctx.shadowBlur  = 5;
  for (let i = 0; i < data.length; i++) {
    const x = (i / (data.length - 1)) * W;
    const y = pad + ((hi - data[i]) / range) * (H - pad * 2);
    i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
  }
  ctx.stroke();
  ctx.shadowBlur = 0;
}

(function animateEcg() {
  drawEcg(ecgData, document.getElementById('leadBadge').dataset.off === 'true');
  requestAnimationFrame(animateEcg);
})();

// ── Helpers ───────────────────────────────────────────────────────────────────
const setText  = (id, v) => { const el = document.getElementById(id); if (el) el.textContent = v; };
const setFill  = (id, p) => { const el = document.getElementById(id); if (el) el.style.width = Math.min(100, Math.max(0, p)) + '%'; };

function setConnected(ok) {
  const pill  = document.getElementById('statusPill');
  const label = document.getElementById('connLabel');
  pill.className    = 'status-pill' + (ok ? '' : ' off');
  label.textContent = ok ? 'Live' : 'Disconnected';
}

// ── Vitals update ─────────────────────────────────────────────────────────────
function updateVitals(d) {
  // ECG badge
  const badge = document.getElementById('leadBadge');
  badge.textContent = d.leadsOff ? 'LEADS OFF' : 'LEADS OK';
  badge.classList.toggle('ok', !d.leadsOff);
  badge.dataset.off = d.leadsOff ? 'true' : 'false';

  // BPM
  const bpmValid = d.bpm > 0 && !d.leadsOff;
  setText('valBpm',    bpmValid ? d.bpm : '--');
  setText('bpmInline', bpmValid ? d.bpm + ' BPM' : '-- BPM');
  setText('subBpm',    d.leadsOff ? 'Attach ECG leads' : bpmValid ? 'Normal range' : 'Awaiting signal');
  setFill('fillBpm',   bpmValid ? ((d.bpm - 40) / 140) * 100 : 0);
  document.getElementById('cardBpm').className = 'card' + (d.leadsOff ? ' alert' : '');

  // SpO2
  const spo2Valid = d.spo2 > 0 && d.fingerOn;
  setText('valSpo2', spo2Valid ? d.spo2 : '--');
  setText('subSpo2', !d.fingerOn ? 'Place finger on sensor' : d.spo2 < 95 ? 'Low — check sensor' : 'Normal');
  setFill('fillSpo2', spo2Valid ? d.spo2 : 0);
  document.getElementById('cardSpo2').className = 'card' +
    (!d.fingerOn ? '' : d.spo2 < 95 ? ' alert' : d.spo2 < 97 ? ' warn' : '');

  // Temp
  const tempDetail = document.getElementById('tempDetail');
  const cardTemp   = document.getElementById('cardTemp');
  if (d.measuring) {
    tempDetail.style.display = 'block';
    document.getElementById('tdBarFill').style.width   = ((30 - d.countdown) / 30 * 100) + '%';
    document.getElementById('tdCountdown').textContent = d.countdown + 's';
    setText('valTemp', '--'); setText('subTemp', 'Measuring…');
    cardTemp.className = 'card';
  } else if (d.tempReady) {
    tempDetail.style.display = 'none';
    setText('valTemp', d.coreTemp.toFixed(1));
    const status = d.coreTemp >= 38 ? 'Fever detected' : d.coreTemp >= 37.5 ? 'Slightly elevated' : 'Normal';
    setText('subTemp', status);
    cardTemp.className = 'card' + (d.coreTemp >= 38 ? ' alert' : d.coreTemp >= 37.5 ? ' warn' : '');
  } else {
    tempDetail.style.display = 'none';
    setText('valTemp', '--'); setText('subTemp', 'Touch sensor to measure');
    cardTemp.className = 'card';
  }

  // Storage
  if (d.storageTotal > 0) {
    const pct  = Math.round((d.storageUsed / d.storageTotal) * 100);
    const kb   = Math.round(d.storageUsed / 1024);
    const info = document.getElementById('storageInfo');
    info.textContent  = `Storage: ${kb} KB used (${pct}%)`;
    info.className    = 'storage-info' + (pct > 90 ? ' full' : pct > 75 ? ' warn' : '');
  }

  // Session controls
  updateSessionUI(d.session);
}

// ── Session UI ────────────────────────────────────────────────────────────────
let lastSessionState = '';

function updateSessionUI(state) {
  if (state === lastSessionState) return;
  lastSessionState = state;

  const dot   = document.getElementById('sessionDot');
  const label = document.getElementById('sessionState');
  const btns  = document.getElementById('sessionBtns');

  dot.className = 'session-dot ' + state;

  if (state === 'idle') {
    label.textContent = 'No active session';
    btns.innerHTML = `
      <button class="btn btn-primary" onclick="sessionAction('start')">▶ Start Session</button>`;

  } else if (state === 'running') {
    label.textContent = 'Recording…';
    btns.innerHTML = `
      <button class="btn btn-amber"   onclick="sessionAction('pause')">⏸ Pause</button>`;

  } else if (state === 'paused') {
    label.textContent = 'Session paused';
    btns.innerHTML = `
      <button class="btn btn-primary"  onclick="sessionAction('resume')">▶ Continue</button>
      <button class="btn btn-outline"  onclick="exportActive()">&#8595; Export CSV</button>
      <button class="btn btn-danger"   onclick="sessionAction('restart')">↺ New Session</button>`;
  }
}

async function sessionAction(action) {
  try {
    const res = await fetch('/session/' + action, { method: 'POST' });
    const d   = await res.json();
    if (action === 'restart' || action === 'pause') loadSessions();
  } catch(e) { console.error(e); }
}

// Export the currently active session (only shown when paused)
async function exportActive() {
  try {
    const res  = await fetch('/sessions');
    const list = await res.json();
    if (list.length === 0) return;
    // Most recent session is the one being exported
    const latest = list[list.length - 1];
    window.location.href = '/download?file=' + latest.file;
  } catch(e) { console.error(e); }
}

// ── Sessions list ─────────────────────────────────────────────────────────────
async function loadSessions() {
  try {
    const res  = await fetch('/sessions');
    const list = await res.json();
    const el   = document.getElementById('sessionsList');

    if (list.length === 0) {
      el.innerHTML = '<p class="empty-msg">No sessions yet</p>';
      return;
    }

    el.innerHTML = list.map(s => `
      <div class="session-row">
        <div class="session-row-info">
          <div class="session-row-name">${s.file}</div>
          <div class="session-row-meta">${Math.round(s.size/1024)} KB &nbsp;·&nbsp; ~${s.rows} rows</div>
        </div>
        <div class="session-row-actions">
          <a href="/download?file=${s.file}" class="btn btn-outline" style="text-decoration:none">&#8595; CSV</a>
          <button class="btn btn-ghost" onclick="deleteSession('${s.file}')">🗑</button>
        </div>
      </div>`).join('');
  } catch(e) { console.error(e); }
}

async function deleteSession(filename) {
  if (!confirm('Delete ' + filename + '?')) return;
  await fetch('/delete?file=' + filename);
  loadSessions();
}

// ── Polling ───────────────────────────────────────────────────────────────────
async function pollEcg() {
  try {
    const res  = await fetch('/ecg', { cache: 'no-store' });
    const data = await res.json();
    if (Array.isArray(data) && data.length === 200) ecgData = data;
  } catch(_) {}
}

let dataFail = 0;
async function pollData() {
  try {
    const res = await fetch('/data', { cache: 'no-store' });
    if (!res.ok) throw new Error();
    const d = await res.json();
    dataFail = 0;
    setConnected(true);
    updateVitals(d);
  } catch(_) {
    if (++dataFail >= 3) setConnected(false);
  }
}

// ── Init ──────────────────────────────────────────────────────────────────────
setConnected(false);
pollData();
pollEcg();
loadSessions();
setInterval(pollData,      1000);
setInterval(pollEcg,        100);
setInterval(loadSessions, 10000);  // refresh session list every 10 s