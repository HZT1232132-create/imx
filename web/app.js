// EdgeGuard-Sort Web Digital Twin Dashboard
const STATE = { events:[], idx:0, autoTimer:null, playing:false, liveMode:false, liveTimer:null, liveUrl:'' };

// ── Init: try to load from events_json/ ──
async function loadJSON() {
  try {
    // Try to load an index file or individual event files
    const files = [];
    for (let i = 1; i <= 20; i++) {
      const sid = String(i).padStart(4,'0');
      try {
        const resp = await fetch(STATE.liveUrl + `/events_json/event_${sid}.json`);
        if (resp.ok) files.push(await resp.json());
        else break;
      } catch(e) { break; }
    }
    if (files.length) {
      STATE.events = files;
      STATE.idx = 0;
      render();
      return;
    }
  } catch(e) { console.log('Auto-load failed, use import'); }
  showStatus('无数据 — 请导入 JSON 文件');
}

function handleFiles(fileList) {
  const readers = [];
  for (const f of fileList) {
    readers.push(new Promise(resolve => {
      const r = new FileReader();
      r.onload = e => { try { resolve(JSON.parse(e.target.result)); } catch(_) { resolve(null); } };
      r.readAsText(f);
    }));
  }
  Promise.all(readers).then(events => {
    STATE.events = events.filter(Boolean).sort((a,b) => (a.frame_id||0) - (b.frame_id||0));
    STATE.idx = 0;
    render();
  });
}

// ── Render ──
function render() {
  const e = STATE.events[STATE.idx];
  if (!e) { showStatus('无事件数据'); return; }

  document.getElementById('frameInfo').textContent = `Frame ${e.frame_id || STATE.idx+1} — ${e.image_name||''}`;
  document.getElementById('progressLabel').textContent = `${STATE.idx+1} / ${STATE.events.length}`;

  // Load image if available
  const canvas = document.getElementById('imageCanvas');
  const ctx = canvas.getContext('2d');
  if (e.image_url) {
    const img = new Image();
    img.onload = () => {
      canvas.width = img.width;
      canvas.height = img.height;
      ctx.drawImage(img, 0, 0);
      drawDetections(ctx, e.detections);
    };
    img.src = e.image_url;
  }

  // A55 pipeline
  setText('a55_pkgId', (e.recognition && e.recognition.final_package_id)||'—');
  const rec = e.recognition || {};
  setText('a55_method', rec.method||'—');
  setText('a55_qr', rec.qr_result||'—');
  setText('a55_ocr', rec.ocr_result||'—');
  setText('a55_target', e.rule ? e.rule.target_zone : '—');
  setText('a55_current', e.rule ? e.rule.current_zone : '—');
  setText('a55_sort', e.rule ? e.rule.sort_status : '—');
  setText('a55_action', e.decision ? e.decision.action : '—');
  setText('a55_conf', e.decision && e.decision.confidence ? (e.decision.confidence*100).toFixed(0)+'%' : '—');

  // Risk level
  const riskEl = document.getElementById('riskLevel');
  const riskMap = { 'LEVEL_0_NORMAL':'🟢 正常', 'LEVEL_1_LOW':'🔵 低风险', 'LEVEL_2_MEDIUM':'🟡 中等', 'LEVEL_3_HIGH':'🟠 高风险', 'LEVEL_4_CRITICAL':'🔴 危险' };
  const rk = e.decision ? e.decision.risk_level||'' : '';
  riskEl.textContent = riskMap[rk] || rk || '—';
  riskEl.className = 'risk-badge risk-' + (rk.includes('0')?0:rk.includes('1')?1:rk.includes('2')?2:rk.includes('3')?3:4);

  // Hash
  setText('hashPrev', 'Prev: '+(e.prev_hash||'—').substring(0,32)+'...');
  setText('hashCurr', 'Curr: '+(e.current_hash||'—').substring(0,32)+'...');
  const hv = document.getElementById('hashVerify');
  hv.textContent = e.verify === 'PASS' ? '✓ HASH PASS' : '✗ HASH FAIL';
  hv.className = 'badge ' + (e.verify === 'PASS' ? 'pass' : 'fail');

  // NPU
  const npu = e.npu || {};
  setText('npu_backend', npu.backend||'—');
  setText('npu_model', npu.model||'—');
  setText('npu_latency', npu.latency_ms ? npu.latency_ms.toFixed(1)+' ms' : '—');
  setText('npu_quality', npu.quality_class||'—');
  setText('npu_conf', npu.confidence ? (npu.confidence*100).toFixed(1)+'%' : '—');
  setText('npu_dets', e.detections ? e.detections.length : 0);

  // M33
  const m33 = e.m33 || {};
  const m33State = document.getElementById('m33_state');
  m33State.textContent = m33.state||'—';
  m33State.className = 'state-badge ' + (
    (m33.state||'').includes('LOCK') ? 'state-lock' :
    (m33.state||'').includes('BLOCK') ? 'state-block' :
    (m33.state||'').includes('SORT') ? 'state-sort' : 'state-idle');

  // LED
  const led = document.getElementById('m33_led');
  led.className = 'led-dot led-' + ({green:'green',yellow:'yellow',red:'red'}[m33.led]||'off');

  setText('m33_buzzer', m33.buzzer||'—');
  setText('m33_motor', m33.motor||'—');
  setText('m33_chute', m33.chute||'—');
  setText('m33_gate', m33.gate||'—');
  setText('m33_heartbeat', m33.heartbeat_ok!==false ? '✓ OK ('+(m33.heartbeat_count||0)+')' : '✗ FAIL');

  // M33 tasks
  const tasksDiv = document.getElementById('m33_tasks');
  const tasks = [
    { name:'CmdRx', ok:m33.command_rx_ok },
    { name:'SortCtrl', ok:m33.sort_control_ok },
    { name:'Safety', ok:m33.safety_task_ok },
    { name:'StatusTx', ok:m33.status_tx_ok },
    { name:'VirtualIO', ok:m33.virtual_io_ok }
  ];
  tasksDiv.innerHTML = tasks.map(t =>
    `<span class="task-chip ${t.ok!==false?'task-ok':'task-err'}">${t.name}:${t.ok!==false?'OK':'ERR'}</span>`
  ).join('');

  // Sort route
  const pkgEl = document.getElementById('route_pkg');
  const chuteEl = document.getElementById('route_chute');
  pkgEl.textContent = (e.recognition && e.recognition.final_package_id)||'PKG—';
  chuteEl.textContent = ({
    'PASS':'滑槽 '+(e.target_zone||'—'),
    'PASS_WITH_LOG':'滑槽 '+(e.target_zone||'—')+' ⚠',
    'REVIEW':'复核区',
    'BLOCK':'拦截'
  })[e.action] || (e.target_zone||'—');

  const chuteClassMap = {
    'PASS':'route-chute-'+((e.target_zone||'a').toLowerCase()),
    'PASS_WITH_LOG':'route-chute-'+((e.target_zone||'a').toLowerCase()),
    'REVIEW':'route-review',
    'BLOCK':'route-block'
  };
  const decision = e.decision || {};
  const rule = e.rule || {};
  chuteEl.className = 'route-item ' + (chuteClassMap[decision.action]||'');
  // Update gates
  updateGates(e.m33||{}, rule.target_zone||'', decision.action||'');
}

// ── Helpers ──
function setText(id, val) { document.getElementById(id).textContent = val; }

function drawDetections(ctx, dets) {
  if (!dets) return;
  const colors = { box:'#38bdf8', label:'#22c55e', barcode:'#fbbf24', qrcode:'#a78bfa', text_region:'#f472b6', damage:'#ef4444' };
  dets.forEach(d => {
    const b = d.bbox;
    ctx.strokeStyle = colors[d.cls] || '#fff';
    ctx.lineWidth = 2;
    ctx.strokeRect(b.x, b.y, b.w||b.width, b.h||b.height);
    ctx.fillStyle = colors[d.cls] || '#fff';
    ctx.font = '12px sans-serif';
    ctx.fillText(`${d.cls} ${(d.confidence*100).toFixed(0)}%`, (b.x||0)+4, (b.y||0)-4);
    if (d.label_text) ctx.fillText(d.label_text, (b.x||0)+4, (b.y||0)+16);
  });
}

function nextFrame() {
  if (STATE.events.length === 0) return;
  STATE.idx = (STATE.idx + 1) % STATE.events.length;
  render();
}
function prevFrame() {
  if (STATE.events.length === 0) return;
  STATE.idx = (STATE.idx - 1 + STATE.events.length) % STATE.events.length;
  render();
}
function toggleAuto() {
  if (STATE.playing) { clearInterval(STATE.autoTimer); STATE.playing = false; document.getElementById('btnAuto').textContent = '▶ 自动播放'; }
  else { STATE.autoTimer = setInterval(nextFrame, 1500); STATE.playing = true; document.getElementById('btnAuto').textContent = '⏸ 暂停'; }
}
function showStatus(msg) {
  document.getElementById('frameInfo').textContent = msg;
}

// ── Live mode: poll latest_event.json and latest_frame.b64 ──
async function fetchLatest() {
  try {
    const resp = await fetch(STATE.liveUrl + '/latest_event.json');
    if (!resp.ok) return;
    const ev = await resp.json();
    // Fetch thumbnail
    try {
      const imgResp = await fetch(STATE.liveUrl + '/latest_frame.b64');
      if (imgResp.ok) {
        const blob = await imgResp.blob();
        const url = URL.createObjectURL(blob);
        const img = new Image();
        img.onload = () => {
          const canvas = document.getElementById('imageCanvas');
          canvas.width = img.width;
          canvas.height = img.height;
          canvas.getContext('2d').drawImage(img, 0, 0);
          if (ev.detections) drawDetections(canvas.getContext('2d'), ev.detections);
        };
        img.src = url;
      }
    } catch(e) {}
    // Push into events and render
    STATE.events = [ev];
    STATE.idx = 0;
    document.getElementById('connection').textContent = '● LIVE';
    document.getElementById('connection').className = 'badge connected';
    renderLive(ev);
  } catch(e) {}
}

function renderLive(e) {
  if (!e) return;
  setText('frameInfo', `LIVE — Frame ${e.frame_id||'?'} — ${e.image_name||''}`);
  setText('a55_pkgId', (e.recognition && e.recognition.final_package_id)||'—');
  const rec = e.recognition || {};
  setText('a55_method', rec.method||'—');
  setText('a55_qr', rec.qr_result||'—');
  setText('a55_ocr', rec.ocr_result||'—');
  setText('a55_target', e.rule ? e.rule.target_zone : '—');
  setText('a55_current', e.rule ? e.rule.current_zone : '—');
  setText('a55_sort', e.rule ? e.rule.sort_status : '—');
  setText('a55_action', e.decision ? e.decision.action : '—');
  setText('a55_conf', e.decision && e.decision.confidence ? (e.decision.confidence*100).toFixed(0)+'%' : '—');

  const riskMap = { 'LEVEL_0_NORMAL':'🟢 正常', 'LEVEL_1_LOW':'🔵 低风险', 'LEVEL_2_MEDIUM':'🟡 中等', 'LEVEL_3_HIGH':'🟠 高风险', 'LEVEL_4_CRITICAL':'🔴 危险' };
  const rk = e.decision ? e.decision.risk_level||'' : '';
  const riskEl = document.getElementById('riskLevel');
  riskEl.textContent = riskMap[rk] || rk || '—';
  riskEl.className = 'risk-badge risk-' + (rk.includes('0')?0:rk.includes('1')?1:rk.includes('2')?2:rk.includes('3')?3:4);

  const h = e.hash || {};
  setText('hashPrev','Prev: '+(h.prev_hash||'—').substring(0,24)+'...');
  setText('hashCurr','Curr: '+(h.current_hash||'—').substring(0,24)+'...');
  const hv = document.getElementById('hashVerify');
  hv.textContent = (h.verify||'')==='PASS'?'✓ HASH PASS':'✗ HASH FAIL';
  hv.className = 'badge ' + ((h.verify||'')==='PASS'?'pass':'fail');

  const npu = e.npu||{};
  setText('npu_backend', npu.backend||'—');
  setText('npu_model', npu.model||'—');
  setText('npu_latency', npu.latency_ms ? npu.latency_ms.toFixed(1)+' ms' : '—');
  setText('npu_quality', npu.quality_class||'—');
  setText('npu_conf', npu.confidence ? (npu.confidence*100).toFixed(1)+'%' : '—');
  setText('npu_dets', e.detections ? e.detections.length : 0);

  const m33 = e.m33||{};
  const ms = document.getElementById('m33_state');
  ms.textContent = m33.state||'—';
  ms.className = 'state-badge ' + ((m33.state||'').includes('LOCK')?'state-lock':(m33.state||'').includes('BLOCK')?'state-block':(m33.state||'').includes('SORT')?'state-sort':'state-idle');
  const led = document.getElementById('m33_led');
  led.className = 'led-dot led-' + ({green:'green',yellow:'yellow',red:'red'}[m33.led]||'off');
  setText('m33_buzzer', m33.buzzer||'—');
  setText('m33_motor', m33.motor||'—');
  setText('m33_chute', m33.chute||'—');
  setText('m33_gate', m33.gate||'—');
  setText('m33_heartbeat', m33.heartbeat_ok!==false ? '✓ OK ('+(m33.heartbeat_count||0)+')' : '✗ FAIL');

  const tasksDiv = document.getElementById('m33_tasks');
  tasksDiv.innerHTML = ['CmdRx','SortCtrl','Safety','StatusTx','VirtualIO'].map(t =>
    `<span class="task-chip ${m33[t.toLowerCase()+'_ok']!==false?'task-ok':'task-err'}">${t}:${m33[t.toLowerCase()+'_ok']!==false?'OK':'ERR'}</span>`
  ).join('');

  const pkgEl = document.getElementById('route_pkg');
  const chuteEl = document.getElementById('route_chute');
  pkgEl.textContent = (e.recognition && e.recognition.final_package_id)||'PKG—';
  chuteEl.textContent = ({'PASS':'滑槽 '+(e.target_zone||'—'),'PASS_WITH_LOG':'滑槽 '+(e.target_zone||'—')+' ⚠','REVIEW':'复核区','BLOCK':'拦截'})[e.action]||(e.target_zone||'—');
  const cm = {'PASS':'route-chute-'+((e.target_zone||'a').toLowerCase()),'PASS_WITH_LOG':'route-chute-'+((e.target_zone||'a').toLowerCase()),'REVIEW':'route-review','BLOCK':'route-block'};
  const d2 = e.decision || {};
  const r2 = e.rule || {};
  chuteEl.className = 'route-item ' + (cm[d2.action]||'');
  updateGates(e.m33||{}, r2.target_zone||'', d2.action||'');
}

function toggleLive() {
  if (STATE.liveMode) {
    clearInterval(STATE.liveTimer);
    STATE.liveMode = false;
    document.getElementById('btnLive').textContent = '🔴 实时';
    document.getElementById('connection').textContent = '● 静态回放';
    document.getElementById('connection').className = 'badge disconnected';
  } else {
    fetchLatest();
    STATE.liveTimer = setInterval(fetchLatest, 500);
    STATE.liveMode = true;
    document.getElementById('btnLive').textContent = '⏸ 停止';
  }
}

// ── Conveyor Belt Animation ──
const BELT = { animId:null, pkgX:20, pkgY:50, pkgOut:false, targetX:20, m33:{}, targetZone:'', action:'' };

function resetPackage() {
  BELT.pkgX = 20;
  BELT.pkgY = 50;
  BELT.pkgOut = false;
}

function drawBeltFrame() {
  const c = document.getElementById('beltCanvas');
  if (!c) { BELT.animId = requestAnimationFrame(drawBeltFrame); return; }
  const ctx = c.getContext('2d');
  const W = c.width, H = c.height;
  ctx.clearRect(0, 0, W, H);

  // Background
  ctx.fillStyle = '#1e293b';
  ctx.fillRect(0, 0, W, H);

  const beltY = 50, beltH = 26;

  // Conveyor belt with scrolling rollers
  const scroll = (Date.now() / 15) % 24;
  ctx.fillStyle = '#334155';
  ctx.fillRect(0, beltY, W, beltH);
  ctx.strokeStyle = '#475569';
  ctx.lineWidth = 1;
  for (let x = -scroll; x < W; x += 24) {
    ctx.beginPath(); ctx.moveTo(x, beltY); ctx.lineTo(x, beltY+beltH); ctx.stroke();
  }
  ctx.strokeStyle = '#64748b';
  ctx.lineWidth = 2;
  ctx.strokeRect(0, beltY, W, beltH);

  // Entry arrow
  ctx.fillStyle = '#475569';
  ctx.beginPath(); ctx.moveTo(5, beltY-12); ctx.lineTo(25, beltY-12); ctx.lineTo(25, beltY-5); ctx.fill();
  ctx.fillStyle = '#64748b';
  ctx.font = '9px sans-serif';
  ctx.fillText('入口', 2, beltY-16);

  // 4 Exit zones
  const exits = [
    { x:160, label:'A', color:'#22c55e' },
    { x:310, label:'B', color:'#38bdf8' },
    { x:460, label:'C', color:'#fbbf24' },
    { x:610, label:'D', color:'#a78bfa' },
    { x:760, label:'复核', color:'#f472b6' }
  ];

  const activeZone = BELT.action === 'REVIEW' ? '复核' : (BELT.targetZone||'');

  exits.forEach(ex => {
    const isActive = (activeZone === ex.label);

    // Chute opening
    ctx.fillStyle = isActive ? ex.color : '#475569';
    ctx.beginPath();
    ctx.moveTo(ex.x-14, beltY+beltH);
    ctx.lineTo(ex.x+14, beltY+beltH);
    ctx.lineTo(ex.x, beltY+beltH+22);
    ctx.closePath();
    ctx.fill();

    // Gate bars: open if active, closed otherwise
    if (isActive) {
      ctx.fillStyle = ex.color;
      ctx.fillRect(ex.x-20, beltY-16, 4, 14);
      ctx.fillRect(ex.x+16, beltY-16, 4, 14);
      ctx.fillRect(ex.x-20, beltY+beltH-20, 4, 14);
      ctx.fillRect(ex.x+16, beltY+beltH-20, 4, 14);
    } else {
      ctx.fillStyle = '#475569';
      ctx.fillRect(ex.x-14, beltY-8, 4, beltH+14);
      ctx.fillRect(ex.x-6, beltY-8, 4, beltH+14);
      ctx.fillRect(ex.x+2, beltY-8, 4, beltH+14);
      ctx.fillRect(ex.x+10, beltY-8, 4, beltH+14);
    }

    ctx.fillStyle = ex.color;
    ctx.font = 'bold 10px sans-serif';
    ctx.textAlign = 'center';
    ctx.fillText(ex.label, ex.x, beltY-18);
  });

  // Package movement
  const arrived = BELT.pkgX >= BELT.targetX - 2 && BELT.pkgX <= BELT.targetX + 2;
  if (arrived && !BELT.pkgOut) {
    BELT.pkgOut = true;
  }
  if (BELT.pkgOut) {
    BELT.pkgY += 1.5;  // slide down through gate
  } else if (!arrived) {
    BELT.pkgX += 1.2;  // move along belt
    if (BELT.pkgX > BELT.targetX) BELT.pkgX = BELT.targetX;
  }

  // Draw package (if not fully exited)
  if (BELT.pkgY < H + 20) {
    const px = BELT.pkgX, py = BELT.pkgY;

    // Shadow
    ctx.fillStyle = 'rgba(0,0,0,0.25)';
    ctx.fillRect(px-13, py+17, 26, 8);

    // Box
    const pkgColor = BELT.action === 'REVIEW' ? '#fbbf24' : '#22c55e';
    ctx.fillStyle = pkgColor;
    ctx.fillRect(px-10, py-5, 20, 20);
    ctx.strokeStyle = '#fff';
    ctx.lineWidth = 1.5;
    ctx.strokeRect(px-10, py-5, 20, 20);

    // Label
    ctx.fillStyle = '#fff';
    ctx.fillRect(px-6, py+1, 12, 7);
    ctx.fillStyle = '#0f172a';
    ctx.font = '5.5px sans-serif';
    ctx.textAlign = 'center';
    ctx.fillText(getCurrentPkgId().substring(0,5), px, py+7);
    ctx.textAlign = 'start';
  }

  // Belt edge highlight
  ctx.strokeStyle = '#38bdf8';
  ctx.lineWidth = 2;
  ctx.strokeRect(0, beltY, W, beltH);

  BELT.animId = requestAnimationFrame(drawBeltFrame);
}

function getCurrentPkgId() {
  const ev = STATE.events[STATE.idx];
  return ev ? ev.final_package_id || 'PKG???' : 'PKG???';
}

function updateGates(m33, targetZone, action) {
  const zoneMap = { 'A':160, 'B':310, 'C':460, 'D':610 };
  BELT.m33 = m33;
  BELT.targetZone = targetZone;
  BELT.action = action;

  resetPackage();

  if (action === 'REVIEW') {
    BELT.targetX = 760;
  } else {
    BELT.targetX = zoneMap[targetZone] || 160;
  }
}

// Start animation loop
requestAnimationFrame(drawBeltFrame);

// Keyboard navigation
document.addEventListener('keydown', e => {
  if (e.key === 'ArrowRight') nextFrame();
  if (e.key === 'ArrowLeft') prevFrame();
  if (e.key === ' ') { e.preventDefault(); toggleAuto(); }
});

loadJSON();
