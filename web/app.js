const UART_SERVICE = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const UART_RX = '6e400002-b5a3-f393-e0a9-e50e24dcca9e';
const UART_TX = '6e400003-b5a3-f393-e0a9-e50e24dcca9e';
let device, rx, tx, buffer = '';
let writeQueue = Promise.resolve();
const statusWaiters = [];

const $ = id => document.getElementById(id);
const numberIds = [
  'focusMinutes','breakMinutes','longBreakMinutes','sessionsBeforeLongBreak',
  'faceMinSeconds','faceMaxSeconds','animationFps','longPressMs','doubleClickMs',
  'petAnimationMs','oledContrast','idleDimSeconds','petSound','doneMelody','breakMelody',
  'personality','accessoryMode','sleepStartHour','sleepEndHour','mpuMountRotation','motionSensitivity',
  'rareEventMinSeconds','rareEventMaxSeconds','idleAnimationSpeed','eyeFollowStrength','inertiaStrength','squashStrength','heartParticleCount','speechEventChance',
  'timerCompanionLayout','microSleepSeconds','speechPack'
];
const stringIds = ['customBootText','customPetText'];
const boolIds = ['autoBreak','soundEnabled','deskBuddyEnabled','touchReactionsEnabled','motionReactionsEnabled','environmentReactionsEnabled','advancedTouchEnabled','rareEventsEnabled','focusCompanionEnabled','speechBubblesEnabled','swapTouchButtons','sleepEnabled','screenFlipped','microSleepEnabled','skinPersonalityEnabled','bootAnimationEnabled'];

function bluetoothApiAvailable() {
  return typeof navigator !== 'undefined' && !!navigator.bluetooth && typeof navigator.bluetooth.requestDevice === 'function';
}

function showBleHelp(lines) {
  const box = $('bleHelp');
  const body = $('bleHelpText');
  if (!box || !body) return;
  if (!lines || !lines.length) {
    box.hidden = true;
    body.innerHTML = '';
    return;
  }
  body.innerHTML = `<ul>${lines.map(line => `<li>${line}</li>`).join('')}</ul>`;
  box.hidden = false;
}

function bluetoothHelpLines() {
  const lines = [];
  if (!window.isSecureContext) {
    lines.push('This page is not in a secure context. Use <code>http://localhost:8080</code> on this computer, or serve it over HTTPS. A LAN URL such as <code>http://192.168.x.x:8080</code> does not qualify.');
  }

  let embedded = false;
  try { embedded = window.top !== window.self; } catch (_) { embedded = true; }
  if (embedded) {
    lines.push('Open the configurator directly in a browser tab instead of an embedded preview or webview.');
  }

  if (!bluetoothApiAvailable()) {
    lines.push('This browser context does not expose <code>navigator.bluetooth</code>. Use desktop Chrome/Chromium with Web Bluetooth enabled.');
    if (/Linux/i.test(navigator.userAgent)) {
      lines.push('On Linux, if the API is still missing, enable <code>chrome://flags/#enable-experimental-web-platform-features</code>, relaunch Chrome/Chromium, then reload this page.');
    }
  }
  return lines;
}

async function initBluetoothSupport() {
  const lines = bluetoothHelpLines();
  const connectButton = $('connect');

  if (lines.length) {
    showBleHelp(lines);
    setConnected(false, 'Web Bluetooth unavailable');
    connectButton.disabled = !bluetoothApiAvailable() || !window.isSecureContext;
    return;
  }

  showBleHelp([]);
  connectButton.disabled = false;
  const bluetooth = bluetoothApiAvailable() ? navigator.bluetooth : null;
  if (bluetooth && typeof bluetooth.getAvailability === 'function') {
    try {
      const available = await bluetooth.getAvailability();
      if (!available) setConnected(false, 'Bluetooth adapter unavailable or disabled');
    } catch (_) {
      // requestDevice() remains the authoritative check after a user gesture.
    }
  }
}

function setConnected(ok, message = ok ? 'Connected' : 'Disconnected') {
  $('status').className = `status ${ok ? 'online' : 'offline'}`;
  $('status').lastChild.textContent = message;
  ['save','reload','factory','refreshStats','refreshSensors','refreshDiagnostics','calibrateMpu','resetMpuCalibration','testPetSound','testDoneMelody','testBreakMelody','testTimerStart','stopSoundPreview'].forEach(id => $(id).disabled = !ok);
  $('connect').textContent = ok ? 'Reconnect' : 'Connect BLE';
}

function cleanTwoWords(value, fallback = '') {
  const words = String(value || '').trim().split(/\s+/).filter(Boolean).slice(0, 2);
  return (words.join(' ') || fallback).slice(0, 19);
}

function formData() {
  const out = {cmd:'set'};
  numberIds.forEach(id => out[id] = Number($(id).value));
  boolIds.forEach(id => out[id] = $(id).checked);
  stringIds.forEach(id => out[id] = cleanTwoWords($(id).value, id === 'customBootText' ? 'Hello!' : 'Happy Happy'));
  return out;
}

function applySettings(s) {
  numberIds.forEach(id => { if (s[id] !== undefined) $(id).value = s[id]; });
  boolIds.forEach(id => { if (s[id] !== undefined) $(id).checked = s[id]; });
  stringIds.forEach(id => { if (s[id] !== undefined) $(id).value = s[id]; });
  if (s.mpuGazeOffsetX !== undefined && s.mpuGazeOffsetY !== undefined) {
    $('calibrationValue').textContent = `${Number(s.mpuGazeOffsetX).toFixed(2)} / ${Number(s.mpuGazeOffsetY).toFixed(2)}`;
  }
  syncOutputs();
  updatePreview();
}

function pct(id, value) {
  $(id).style.width = `${Math.max(0, Math.min(100, Number(value) || 0))}%`;
}

function applyState(s) {
  $('moodValue').textContent = s.mood ?? '--';
  $('energyValue').textContent = s.energy ?? '--';
  $('affectionValue').textContent = s.affection ?? '--';
  pct('moodBar', s.mood);
  pct('energyBar', s.energy);
  pct('affectionBar', s.affection);
  $('levelValue').textContent = s.level ?? '--';
  $('xpValue').textContent = s.xp ?? '--';
  $('streakValue').textContent = `${s.streakDays ?? 0}d`;
  $('focusTotalValue').textContent = `${s.focusSessions ?? 0} / ${s.focusMinutes ?? 0}m`;
  $('todayValue').textContent = `${s.todaySessions ?? 0} / ${s.todayMinutes ?? 0}m`;
  $('interactionsValue').textContent = `${s.pets ?? 0} pets · ${s.boops ?? 0} boops`;
  $('touchStatsValue').textContent = `${s.hugs ?? 0} hugs · ${s.scratches ?? 0} scratches · ${s.swipes ?? 0} swipes`;
  $('focusXpValue').textContent = s.focusXp ?? 0;
  $('rareEventsValue').textContent = s.rareEvents ?? 0;
  renderFocusHistory(s.historyDay || [], s.historySessions || [], s.historyMinutes || []);

  const level = Number(s.level || 1);
  const unlocked = level >= 7 ? 'Crown' : level >= 4 ? 'Orbit' : level >= 2 ? 'Spark' : 'None yet';
  $('unlockText').textContent = `Level ${level} · best unlocked: ${unlocked}`;
}

function renderFocusHistory(days, sessions, minutes) {
  const host = $('focusHistory');
  if (!host) return;
  const entries = [];
  for (let i = 0; i < Math.min(7, days.length); i++) {
    if (Number(days[i]) < 0) continue;
    entries.push({day:Number(days[i]), sessions:Number(sessions[i] || 0), minutes:Number(minutes[i] || 0)});
  }
  entries.sort((a,b) => a.day - b.day);
  const maxMinutes = Math.max(1, ...entries.map(x => x.minutes));
  host.innerHTML = '';
  if (!entries.length) {
    host.innerHTML = '<p class="micro">Connect and complete a focus session to start the history.</p>';
    return;
  }
  entries.forEach((entry, index) => {
    const item = document.createElement('div');
    item.className = 'history-day';
    const height = Math.max(6, Math.round((entry.minutes / maxMinutes) * 72));
    item.innerHTML = `<b style="height:${height}px"></b><span>D${index + 1}</span><em>${entry.minutes}m</em>`;
    item.title = `${entry.sessions} session(s), ${entry.minutes} min`;
    host.appendChild(item);
  });
}

function applyDiagnostics(d) {
  $('diagLoopHz').textContent = `${Number(d.loopHz || 0).toFixed(0)} Hz`;
  $('diagRenderFps').textContent = `${Number(d.renderFps || 0).toFixed(1)} fps`;
  $('diagPresented').textContent = Number(d.framesPresented || 0).toLocaleString();
  $('diagSkipped').textContent = Number(d.framesSkipped || 0).toLocaleString();
  const sec = Math.floor(Number(d.uptimeMs || 0) / 1000);
  $('diagUptime').textContent = `${Math.floor(sec / 3600)}h ${Math.floor((sec % 3600) / 60)}m`;
}

function applySensors(s) {
  const mpu = !!s.mpuAvailable;
  const bmp = !!s.bmpAvailable;
  $('mpuStatus').textContent = mpu ? 'Online' : 'Not found';
  $('bmpStatus').textContent = bmp ? 'Online' : 'Not found';
  $('tiltValue').textContent = mpu ? `${Number(s.gazeX || 0).toFixed(2)} / ${Number(s.gazeY || 0).toFixed(2)}` : '--';
  $('motionValue').textContent = mpu ? `${Math.round((Number(s.motion) || 0) * 100)}%` : '--';
  $('motionVectorValue').textContent = mpu ? `${Number(s.linearX || 0).toFixed(2)} / ${Number(s.linearY || 0).toFixed(2)} m/s²` : '--';
  $('gravityValue').textContent = mpu ? `${Number(s.gravityX || 0).toFixed(2)} / ${Number(s.gravityY || 0).toFixed(2)} / ${Number(s.gravityZ || 0).toFixed(2)}` : '--';
  if (!mpu) {
    $('restCalibrationValue').textContent = '--';
  } else if (s.restCalibrated) {
    $('restCalibrationValue').textContent = 'Ready';
  } else {
    const seconds = Math.min(10, Math.max(0, Number(s.restCalProgressMs || 0) / 1000));
    $('restCalibrationValue').textContent = `Keep still ${seconds.toFixed(1)} / 10s`;
  }
  $('tempValue').textContent = bmp && s.temperatureC != null && Number.isFinite(Number(s.temperatureC)) ? `${Number(s.temperatureC).toFixed(1)} °C` : '--';
  $('pressureValue').textContent = bmp && s.pressureHpa != null && Number.isFinite(Number(s.pressureHpa)) ? `${Number(s.pressureHpa).toFixed(0)} hPa` : '--';
}

function syncOutputs() {
  $('fpsOut').value = $('animationFps').value;
  $('contrastOut').value = $('oledContrast').value;
  $('motionSensitivityOut').value = `${$('motionSensitivity').value}%`;
  $('idleAnimationSpeedOut').value = `${$('idleAnimationSpeed').value}%`;
  $('eyeFollowStrengthOut').value = `${$('eyeFollowStrength').value}%`;
  $('inertiaStrengthOut').value = `${$('inertiaStrength').value}%`;
  $('squashStrengthOut').value = `${$('squashStrength').value}%`;
  $('heartParticleCountOut').value = $('heartParticleCount').value;
  $('speechEventChanceOut').value = `${$('speechEventChance').value}%`;
}

function updatePreview() {
  const face = document.querySelector('.pet-preview');
  face.dataset.style = $('personality').value;
}

function delay(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

function queueWrite(operation) {
  const next = writeQueue.then(operation, operation);
  writeQueue = next.catch(() => {});
  return next;
}

async function writeRawNow(text, {reliable = false, interChunkDelay = 0} = {}) {
  if (!rx || !device?.gatt?.connected) throw new Error('Pet is not connected');

  const data = new TextEncoder().encode(text);
  const chunkSize = 20;
  for (let i = 0; i < data.length; i += chunkSize) {
    const chunk = data.slice(i, i + chunkSize);
    if (reliable && typeof rx.writeValueWithResponse === 'function') {
      await rx.writeValueWithResponse(chunk);
    } else if (typeof rx.writeValueWithoutResponse === 'function') {
      await rx.writeValueWithoutResponse(chunk);
    } else {
      await rx.writeValue(chunk);
    }
    if (interChunkDelay && i + chunkSize < data.length) await delay(interChunkDelay);
  }
}

function sendRaw(text, options = {}) {
  return queueWrite(() => writeRawNow(text, options));
}

function send(obj, options = {}) {
  return sendRaw(JSON.stringify(obj) + '\n', options);
}

function waitForDeviceStatus(expectedDetails, timeoutMs = 5000) {
  const expected = new Set(Array.isArray(expectedDetails) ? expectedDetails : [expectedDetails]);
  return new Promise((resolve, reject) => {
    const waiter = {expected, resolve, reject, timer: null};
    waiter.timer = setTimeout(() => {
      const index = statusWaiters.indexOf(waiter);
      if (index >= 0) statusWaiters.splice(index, 1);
      reject(new Error(`No device acknowledgement (${[...expected].join(', ')})`));
    }, timeoutMs);
    statusWaiters.push(waiter);
  });
}

function dispatchDeviceStatus(msg) {
  for (let i = statusWaiters.length - 1; i >= 0; i--) {
    const waiter = statusWaiters[i];
    if (!waiter.expected.has(msg.detail)) continue;
    clearTimeout(waiter.timer);
    statusWaiters.splice(i, 1);
    waiter.resolve(msg);
  }
}

function rejectStatusWaiters(reason = 'Disconnected') {
  while (statusWaiters.length) {
    const waiter = statusWaiters.pop();
    clearTimeout(waiter.timer);
    waiter.reject(new Error(reason));
  }
}

function onNotify(event) {
  buffer += new TextDecoder().decode(event.target.value);
  let idx;
  while ((idx = buffer.indexOf('\n')) >= 0) {
    const line = buffer.slice(0, idx).trim();
    buffer = buffer.slice(idx + 1);
    if (!line) continue;
    try {
      const msg = JSON.parse(line);
      if (msg.type === 'settings') applySettings(msg);
      if (msg.type === 'state') applyState(msg);
      if (msg.type === 'sensors') applySensors(msg);
      if (msg.type === 'diagnostics') applyDiagnostics(msg);
      if (msg.type === 'status') {
        dispatchDeviceStatus(msg);
        const label = msg.status === 'error' ? `Device error: ${msg.detail || 'unknown'}` : (msg.detail || msg.status);
        setConnected(true, label);
      }
    } catch (e) {
      console.warn('Invalid device message', line);
    }
  }
}

async function syncTime() {
  await send({
    cmd:'time',
    epoch:Math.floor(Date.now() / 1000),
    tzOffsetMinutes:-new Date().getTimezoneOffset()
  });
}

async function connect() {
  if (!bluetoothApiAvailable()) {
    setConnected(false, 'Web Bluetooth unavailable');
    showBleHelp(bluetoothHelpLines());
    return;
  }

  if (!window.isSecureContext) {
    setConnected(false, 'Secure context required');
    showBleHelp(bluetoothHelpLines());
    return;
  }

  try {
    showBleHelp([]);
    setConnected(false, 'Select your XIAO…');
    const bluetooth = navigator.bluetooth;
    device = await bluetooth.requestDevice({filters:[{services:[UART_SERVICE]}]});
    device.addEventListener('gattserverdisconnected', () => {
      rx = tx = null;
      writeQueue = Promise.resolve();
      rejectStatusWaiters('Pet disconnected');
      setConnected(false);
    });
    const server = await device.gatt.connect();
    const service = await server.getPrimaryService(UART_SERVICE);
    rx = await service.getCharacteristic(UART_RX);
    tx = await service.getCharacteristic(UART_TX);
    await tx.startNotifications();
    tx.addEventListener('characteristicvaluechanged', onNotify);
    setConnected(true);
    await syncTime();
    await send({cmd:'get'});
  } catch (e) {
    rx = tx = null;
    writeQueue = Promise.resolve();

    if (e?.name === 'NotFoundError') {
      // Cancelling Chrome's chooser is a normal user action, not an error.
      setConnected(false, 'Connection cancelled');
      return;
    }

    console.warn('BLE connection failed', e);
    let message = e?.message || 'Connection failed';
    if (e?.name === 'SecurityError' || e?.name === 'NotAllowedError') message = 'Bluetooth access blocked by browser security';
    else if (e?.name === 'NetworkError') message = 'Bluetooth connection failed';
    setConnected(false, message);
    if (e?.name === 'SecurityError' || e?.name === 'NotAllowedError') showBleHelp(bluetoothHelpLines());
  }
}

$('connect').addEventListener('click', connect);

async function runBleAction(label, action) {
  try {
    await action();
  } catch (e) {
    console.warn(label, e);
    if (e?.name === 'NetworkError' || !device?.gatt?.connected) {
      rx = tx = null;
      rejectStatusWaiters('Pet disconnected');
      setConnected(false, 'Connection lost');
    } else {
      setConnected(true, e?.message || label);
    }
  }
}

$('save').addEventListener('click', () => runBleAction('Save failed', async () => {
  setConnected(true, 'Saving…');
  $('save').disabled = true;
  try {
    const acknowledgement = waitForDeviceStatus(['saved', 'save_failed'], 6000);
    await send(formData(), {reliable: true});
    const result = await acknowledgement;
    if (result.detail !== 'saved') throw new Error('Pet could not save settings');

    // Let the settings response finish before sending follow-up commands.
    await delay(80);
    await syncTime();
    await send({cmd:'state'}, {reliable: true});
    setConnected(true, 'Saved');
  } finally {
    if (device?.gatt?.connected) $('save').disabled = false;
  }
}));

$('reload').addEventListener('click', () => runBleAction('Reload failed', async () => {
  await syncTime();
  await send({cmd:'get'}, {reliable: true});
}));

$('refreshStats').addEventListener('click', () => runBleAction('Stats refresh failed', () => send({cmd:'state'})));
$('refreshSensors').addEventListener('click', () => runBleAction('Sensor refresh failed', () => send({cmd:'sensors'})));
$('refreshDiagnostics').addEventListener('click', () => runBleAction('Diagnostics refresh failed', () => send({cmd:'diagnostics'})));
$('calibrateMpu').addEventListener('click', () => runBleAction('MPU calibration failed', async () => {
  setConnected(true, 'Keep the pet still…');
  const done = waitForDeviceStatus(['mpu_calibrated','mpu_not_found'], 4000);
  await send({cmd:'calibrate_mpu'}, {reliable:true});
  const result = await done;
  if (result.detail !== 'mpu_calibrated') throw new Error('MPU6050 not found');
  await send({cmd:'get'});
  setConnected(true, 'MPU calibrated');
}));
$('resetMpuCalibration').addEventListener('click', () => runBleAction('MPU reset failed', async () => {
  const done = waitForDeviceStatus(['mpu_calibration_reset','mpu_not_found'], 4000);
  await send({cmd:'reset_mpu_calibration'}, {reliable:true});
  const result = await done;
  if (result.detail !== 'mpu_calibration_reset') throw new Error('MPU6050 not found');
  await send({cmd:'get'});
  setConnected(true, 'MPU calibration reset');
}));

// Compact one-packet commands make previews feel immediate.
$('testPetSound').addEventListener('click', () => runBleAction('Preview failed', () => sendRaw(`Pp${Number($('petSound').value)}\n`)));
$('testDoneMelody').addEventListener('click', () => runBleAction('Preview failed', () => sendRaw(`Pd${Number($('doneMelody').value)}\n`)));
$('testBreakMelody').addEventListener('click', () => runBleAction('Preview failed', () => sendRaw(`Pb${Number($('breakMelody').value)}\n`)));
$('testTimerStart').addEventListener('click', () => runBleAction('Preview failed', () => sendRaw('Pt0\n')));
$('stopSoundPreview').addEventListener('click', () => runBleAction('Stop preview failed', () => sendRaw('PX\n')));

$('factory').addEventListener('click', () => {
  if (!confirm('Reset settings, XP and all stats?')) return;
  runBleAction('Factory reset failed', () => send({cmd:'factory'}, {reliable: true}));
});
$('animationFps').addEventListener('input', syncOutputs);
$('oledContrast').addEventListener('input', syncOutputs);
$('motionSensitivity').addEventListener('input', syncOutputs);
['idleAnimationSpeed','eyeFollowStrength','inertiaStrength','squashStrength','heartParticleCount','speechEventChance'].forEach(id => $(id).addEventListener('input', syncOutputs));
$('personality').addEventListener('change', updatePreview);

const PRESETS = {
  calm: {animationFps:30, idleAnimationSpeed:75, eyeFollowStrength:80, inertiaStrength:70, squashStrength:55, heartParticleCount:6, rareEventsEnabled:true, speechEventChance:25, focusCompanionEnabled:true},
  expressive: {animationFps:40, idleAnimationSpeed:115, eyeFollowStrength:115, inertiaStrength:110, squashStrength:110, heartParticleCount:12, rareEventsEnabled:true, speechEventChance:55, focusCompanionEnabled:true},
  cartoon: {animationFps:50, idleAnimationSpeed:125, eyeFollowStrength:120, inertiaStrength:140, squashStrength:140, heartParticleCount:16, rareEventsEnabled:true, speechEventChance:50, focusCompanionEnabled:true},
  minimal: {animationFps:30, idleAnimationSpeed:70, eyeFollowStrength:70, inertiaStrength:60, squashStrength:35, heartParticleCount:3, rareEventsEnabled:false, speechEventChance:10, focusCompanionEnabled:false},
  focus: {animationFps:30, idleAnimationSpeed:65, eyeFollowStrength:75, inertiaStrength:65, squashStrength:45, heartParticleCount:5, rareEventsEnabled:false, speechEventChance:15, focusCompanionEnabled:true}
};

document.querySelectorAll('[data-preset]').forEach(button => {
  button.addEventListener('click', () => {
    const preset = PRESETS[button.dataset.preset];
    Object.entries(preset).forEach(([id, value]) => {
      const el = $(id);
      if (!el) return;
      if (typeof value === 'boolean') el.checked = value;
      else el.value = value;
    });
    syncOutputs();
  });
});

$('exportConfig').addEventListener('click', () => {
  const data = formData();
  delete data.cmd;
  const blob = new Blob([JSON.stringify({format:'xiao-computer-pet-config', version:1, settings:data}, null, 2)], {type:'application/json'});
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = 'xiao-computer-pet-config.json';
  a.click();
  setTimeout(() => URL.revokeObjectURL(url), 500);
});

$('importConfig').addEventListener('click', () => $('importConfigFile').click());
$('importConfigFile').addEventListener('change', async event => {
  const file = event.target.files?.[0];
  event.target.value = '';
  if (!file) return;
  try {
    const parsed = JSON.parse(await file.text());
    const data = parsed.settings || parsed;
    numberIds.forEach(id => { if (data[id] !== undefined) $(id).value = data[id]; });
    boolIds.forEach(id => { if (data[id] !== undefined) $(id).checked = !!data[id]; });
    stringIds.forEach(id => { if (data[id] !== undefined) $(id).value = cleanTwoWords(data[id]); });
    syncOutputs();
    updatePreview();
    setConnected(!!device?.gatt?.connected, 'Configuration imported · Save to pet to apply');
  } catch (error) {
    console.warn('Config import failed', error);
    setConnected(!!device?.gatt?.connected, 'Invalid configuration file');
  }
});

stringIds.forEach(id => $(id).addEventListener('change', () => {
  $(id).value = cleanTwoWords($(id).value, id === 'customBootText' ? 'Hello!' : 'Happy Happy');
}));

syncOutputs();
updatePreview();

initBluetoothSupport();
