'use strict';

// ── ECG canvas ────────────────────────────────────────────────────────────────
const canvas = document.getElementById('ecgCanvas');
const ctx    = canvas.getContext('2d');
let ecgData  = new Array(200).fill(2048);

function resizeCanvas() {
  canvas.width  = canvas.offsetWidth;
  canvas.height = canvas.offsetHeight;
}
resizeCanvas();
window.addEventListener('resize', resizeCanvas);

function drawEcg(data, leadsOff) {
  const W = canvas.width, H = canvas.height;
  ctx.clearRect(0, 0, W, H);

  if (leadsOff) {
    ctx.fillStyle = '#475569';
    ctx.font = '13px "Segoe UI", system-ui, sans-serif';
    ctx.textAlign = 'center';
    ctx.fillText('— LEADS OFF —', W / 2, H / 2);
    return;
  }

  let lo = Infinity, hi = -Infinity;
  for (let i = 0; i < data.length; i++) {
    if (data[i] < lo) lo = data[i];
    if (data[i] > hi) hi = data[i];
  }
  const range = hi - lo || 1;
  const pad = H * 0.1;

  ctx.beginPath();
  ctx.strokeStyle = '#3b82f6';   // blue trace — matches design
  ctx.lineWidth   = 2;
  ctx.shadowColor = '#3b82f6';
  ctx.shadowBlur  = 6;

  for (let i = 0; i < data.length; i++) {
    const x = (i / (data.length - 1)) * W;
    const y = pad + ((hi - data[i]) / range) * (H - pad * 2);
    i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
  }
  ctx.stroke();
  ctx.shadowBlur = 0;
}

function animateEcg() {
  const leadsOff = document.getElementById('leadBadge').dataset.off === 'true';
  drawEcg(ecgData, leadsOff);
  requestAnimationFrame(animateEcg);
}
animateEcg();

// ── Helpers ───────────────────────────────────────────────────────────────────
function setText(id, val) { document.getElementById(id).textContent = val; }
function setFill(id, pct) {
  document.getElementById(id).style.width = Math.min(100, Math.max(0, pct)) + '%';
}

function setConnected(ok) {
  const pill  = document.getElementById('statusPill');
  const label = document.getElementById('connLabel');
  pill.className    = 'status-pill' + (ok ? '' : ' off');
  label.textContent = ok ? 'Live' : 'Disconnected';
}

// ── ECG polling — 100 ms ──────────────────────────────────────────────────────
async function pollEcg() {
  try {
    const res  = await fetch('/ecg', { cache: 'no-store' });
    const data = await res.json();
    if (Array.isArray(data) && data.length === 200) ecgData = data;
  } catch (_) {}
}

// ── Data polling — 1 s ───────────────────────────────────────────────────────
let dataFail = 0;

async function pollData() {
  try {
    const res = await fetch('/data', { cache: 'no-store' });
    if (!res.ok) throw new Error();
    const d = await res.json();
    dataFail = 0;
    setConnected(true);
    updateVitals(d);
  } catch (_) {
    if (++dataFail >= 3) setConnected(false);
  }
}

function updateVitals(d) {
  // ── ECG badge ─────────────────────────────────────────────────────────────
  const badge = document.getElementById('leadBadge');
  if (d.leadsOff) {
    badge.textContent  = 'LEADS OFF';
    badge.classList.remove('ok');
    badge.dataset.off  = 'true';
  } else {
    badge.textContent  = 'LEADS OK';
    badge.classList.add('ok');
    badge.dataset.off  = 'false';
  }

  // ── BPM ──────────────────────────────────────────────────────────────────
  const bpmValid = d.bpm > 0 && !d.leadsOff;
  setText('valBpm',    bpmValid ? d.bpm : '--');
  setText('bpmInline', bpmValid ? d.bpm + ' BPM' : '-- BPM');
  setText('subBpm',    d.leadsOff ? 'Attach ECG leads' : (bpmValid ? 'Normal range' : 'Awaiting signal'));
  setFill('fillBpm',   bpmValid ? ((d.bpm - 40) / 140) * 100 : 0);

  const cardBpm = document.getElementById('cardBpm');
  cardBpm.className = 'card' + (d.leadsOff ? ' alert' : '');

  // ── SpO2 ─────────────────────────────────────────────────────────────────
  const spo2Valid = d.spo2 > 0 && d.fingerOn;
  setText('valSpo2', spo2Valid ? d.spo2 : '--');
  setText('subSpo2', !d.fingerOn ? 'Place finger on sensor'
                   : d.spo2 < 95 ? 'Low — check sensor'
                   : 'Normal');
  setFill('fillSpo2', spo2Valid ? d.spo2 : 0);

  const cardSpo2 = document.getElementById('cardSpo2');
  cardSpo2.className = 'card' + (!d.fingerOn ? '' : d.spo2 < 95 ? ' alert' : d.spo2 < 97 ? ' warn' : '');

  // ── Temperature ──────────────────────────────────────────────────────────
  const tempDetail = document.getElementById('tempDetail');
  const cardTemp   = document.getElementById('cardTemp');

  if (d.measuring) {
    tempDetail.style.display = 'block';
    const pct = ((30 - d.countdown) / 30) * 100;
    document.getElementById('tdBarFill').style.width   = pct + '%';
    document.getElementById('tdCountdown').textContent = d.countdown + 's';
    setText('valTemp', '--');
    setText('subTemp', 'Measuring…');
    cardTemp.className = 'card';
  } else if (d.tempReady) {
    tempDetail.style.display = 'none';
    setText('valTemp', d.coreTemp.toFixed(1));
    const status = d.coreTemp >= 38.0 ? 'Fever detected'
                 : d.coreTemp >= 37.5 ? 'Slightly elevated'
                 : 'Normal';
    setText('subTemp', status);
    cardTemp.className = 'card' + (d.coreTemp >= 38.0 ? ' alert' : d.coreTemp >= 37.5 ? ' warn' : '');
  } else {
    tempDetail.style.display = 'none';
    setText('valTemp', '--');
    setText('subTemp', 'Touch sensor to measure');
    cardTemp.className = 'card';
  }
}

// ── Start ─────────────────────────────────────────────────────────────────────
setConnected(false);   // start as disconnected, not "Connecting"
pollData();
pollEcg();
setInterval(pollData, 1000);
setInterval(pollEcg,   100);