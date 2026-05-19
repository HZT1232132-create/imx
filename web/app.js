// EdgeGuard-Sort Web Digital Twin Dashboard
const STATE = { events:[], idx:0, autoTimer:null, playing:false, liveMode:false, liveTimer:null, liveUrl:'../output' };

// ── Init: try to load from events_json/ ──
async function loadJSON() {
  try {
    // Try to load an index file or individual event files
    const files = [];
    for (let i = 1; i <= 20; i++) {
      const sid = String(i).padStart(4,'0');
      try {
        const resp = await fetch(`../output/events_json/event_${sid}.json`);
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
  setText('a55_pkgId', e.final_package_id||'—');
  setText('a55_method', e.recognition_method||'—');
  setText('a55_qr', e.qr_result||'—');
  setText('a55_ocr', e.ocr_result||'—');
  setText('a55_target', e.target_zone||'—');
  setText('a55_current', e.current_zone||'—');
  setText('a55_sort', e.sort_status||'—');
  setText('a55_action', e.action||'—');
  setText('a55_conf', e.decision_confidence ? (e.decision_confidence*100).toFixed(0)+'%' : '—');

  // Risk level
  const riskEl = document.getElementById('riskLevel');
  const riskMap = { 'LEVEL_0_NORMAL':'🟢 正常', 'LEVEL_1_LOW':'🔵 低风险', 'LEVEL_2_MEDIUM':'🟡 中等', 'LEVEL_3_HIGH':'🟠 高风险', 'LEVEL_4_CRITICAL':'🔴 危险' };
  const rk = e.risk_level||'';
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
  pkgEl.textContent = e.final_package_id||'PKG—';
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
  chuteEl.className = 'route-item ' + (chuteClassMap[e.action]||'');
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
  setText('a55_pkgId', e.final_package_id||'—');
  setText('a55_method', e.recognition_method||'—');
  setText('a55_qr', e.qr_result||'—');
  setText('a55_ocr', e.ocr_result||'—');
  setText('a55_target', e.target_zone||'—');
  setText('a55_current', e.current_zone||'—');
  setText('a55_sort', e.sort_status||'—');
  setText('a55_action', e.action||'—');
  setText('a55_conf', e.decision_confidence ? (e.decision_confidence*100).toFixed(0)+'%' : '—');

  const riskMap = { 'LEVEL_0_NORMAL':'🟢 正常', 'LEVEL_1_LOW':'🔵 低风险', 'LEVEL_2_MEDIUM':'🟡 中等', 'LEVEL_3_HIGH':'🟠 高风险', 'LEVEL_4_CRITICAL':'🔴 危险' };
  const rk = e.risk_level||'';
  const riskEl = document.getElementById('riskLevel');
  riskEl.textContent = riskMap[rk] || rk || '—';
  riskEl.className = 'risk-badge risk-' + (rk.includes('0')?0:rk.includes('1')?1:rk.includes('2')?2:rk.includes('3')?3:4);

  setText('hashPrev','Prev: '+(e.prev_hash||'—').substring(0,24)+'...');
  setText('hashCurr','Curr: '+(e.current_hash||'—').substring(0,24)+'...');
  const hv = document.getElementById('hashVerify');
  hv.textContent = e.verify==='PASS'?'✓ HASH PASS':'✗ HASH FAIL';
  hv.className = 'badge ' + (e.verify==='PASS'?'pass':'fail');

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
  pkgEl.textContent = e.final_package_id||'PKG—';
  chuteEl.textContent = ({'PASS':'滑槽 '+(e.target_zone||'—'),'PASS_WITH_LOG':'滑槽 '+(e.target_zone||'—')+' ⚠','REVIEW':'复核区','BLOCK':'拦截'})[e.action]||(e.target_zone||'—');
  const cm = {'PASS':'route-chute-'+((e.target_zone||'a').toLowerCase()),'PASS_WITH_LOG':'route-chute-'+((e.target_zone||'a').toLowerCase()),'REVIEW':'route-review','BLOCK':'route-block'};
  chuteEl.className = 'route-item ' + (cm[e.action]||'');
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

// Keyboard navigation
document.addEventListener('keydown', e => {
  if (e.key === 'ArrowRight') nextFrame();
  if (e.key === 'ArrowLeft') prevFrame();
  if (e.key === ' ') { e.preventDefault(); toggleAuto(); }
});

loadJSON();
