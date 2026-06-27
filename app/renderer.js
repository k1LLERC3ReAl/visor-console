'use strict'

/* ── State ──────────────────────────────────────────────────────────────── */

const State = {
  IDLE:       'idle',
  CONNECTING: 'connecting',
  MONITORING: 'monitoring',
  DIAGNOSTIC: 'diagnostic',
  RESTORING:  'restoring',
  ERROR:      'error',
}

let appState    = State.IDLE
let sensorCount = 8
let posMin      = -7000
let posMax      = 7000
let frozen      = false
let lastSensor  = null

/* ── DOM refs ───────────────────────────────────────────────────────────── */

const $ = id => document.getElementById(id)

const els = {
  connDot:        $('conn-dot'),
  connLabel:      $('conn-label'),

  selPort:        $('sel-port'),
  selBaudrate:    $('sel-baudrate'),
  selBoard:       $('sel-board'),
  selBartype:     $('sel-bartype'),
  inpThrBlack:    $('inp-thr-black'),
  inpThrWhite:    $('inp-thr-white'),
  inpTimeout:     $('inp-timeout'),

  btnRefreshPorts:$('btn-refresh-ports'),
  btnSave:        $('btn-save'),
  btnMonitor:     $('btn-monitor'),
  btnDiagnostic:  $('btn-diagnostic'),
  btnStop:        $('btn-stop'),
  btnErrorOk:     $('btn-error-ok'),
  btnClearLog:    $('btn-clear-log'),

  stateIdle:      $('state-idle'),
  stateWaiting:   $('state-waiting'),
  stateCalib:     $('state-calibrating'),
  stateMonitor:   $('state-monitor'),
  stateDiag:      $('state-diagnostic'),
  stateRestoring: $('state-restoring'),
  stateError:     $('state-error'),

  calibSpinner:   $('calib-spinner'),
  calibDone:      $('calib-done-badge'),

  waitingMsg:     $('waiting-msg'),
  monBarName:     $('mon-bar-name'),
  monPort:        $('mon-port'),
  monBaud:        $('mon-baud'),
  freezeBadge:    $('freeze-badge'),
  sensorGrid:     $('sensor-grid'),
  posValue:       $('pos-value'),
  posNeedle:      $('pos-needle'),
  posMin:         $('pos-min'),
  posMax:         $('pos-max'),
  sumNegro:       $('sum-negro'),
  sumGris:        $('sum-gris'),
  sumBlanco:      $('sum-blanco'),
  timeoutBadge:   $('timeout-badge'),
  diagSensorArea: $('diag-sensor-area'),
  errorMsg:       $('error-msg'),
  configDisplay:  $('config-display'),
  logOutput:      $('log-output'),

  barThumb:          $('bar-thumb'),
  idleBarImg:        $('idle-bar-img'),
  calibBarImg:       $('calib-bar-img'),
}

const tabPanels = {
  dashboard: $('tab-dashboard'),
  config:    $('tab-config'),
  log:       $('tab-log'),
}

/* ── Tab navigation ─────────────────────────────────────────────────────── */

document.querySelectorAll('.tab-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'))
    btn.classList.add('active')
    const tab = btn.dataset.tab
    Object.entries(tabPanels).forEach(([key, panel]) => {
      panel.style.display = key === tab ? 'flex' : 'none'
      if (key === tab) panel.classList.add('active')
      else panel.classList.remove('active')
    })
  })
})

/* ── State panels ───────────────────────────────────────────────────────── */

function showPanel(name) {
  [els.stateIdle, els.stateWaiting, els.stateCalib, els.stateMonitor,
   els.stateDiag, els.stateRestoring, els.stateError].forEach(p => {
    p.style.display = 'none'
  })
  const map = {
    idle:        els.stateIdle,
    waiting:     els.stateWaiting,
    calibrating: els.stateCalib,
    monitor:     els.stateMonitor,
    diagnostic:  els.stateDiag,
    restoring:   els.stateRestoring,
    error:       els.stateError,
  }
  if (map[name]) map[name].style.display = 'flex'
}

function setState(s) {
  appState = s
  const isRunning = s === State.MONITORING || s === State.DIAGNOSTIC || s === State.RESTORING
  els.btnStop.style.display    = isRunning ? 'flex' : 'none'
  els.btnMonitor.style.display = isRunning ? 'none' : 'flex'
  els.btnDiagnostic.style.display = isRunning ? 'none' : 'flex'
  setSidebarEnabled(!isRunning)
  updateConnUi()
}

function setSidebarEnabled(enabled) {
  ;[els.selPort, els.selBaudrate, els.selBoard, els.selBartype,
    els.inpThrBlack, els.inpThrWhite, els.inpTimeout,
    els.btnSave, els.btnRefreshPorts].forEach(el => {
    el.disabled = !enabled
  })
}

function updateConnUi() {
  const states = {
    [State.IDLE]:       { dot: 'disconnected', label: 'Sin conexión' },
    [State.CONNECTING]: { dot: 'connecting',   label: 'Conectando...' },
    [State.MONITORING]: { dot: 'connected',    label: 'Monitoreando' },
    [State.DIAGNOSTIC]: { dot: 'connecting',   label: 'Diagnóstico' },
    [State.RESTORING]:  { dot: 'connecting',   label: 'Restaurando...' },
    [State.ERROR]:      { dot: 'error',        label: 'Error' },
  }
  const info = states[appState] || states[State.IDLE]
  els.connDot.className = 'conn-dot ' + info.dot
  els.connLabel.textContent = info.label
}

/* ── Sensor grid ────────────────────────────────────────────────────────── */

const STATUS = { 0: 'unknown', 1: 'blanco', 2: 'gris', 3: 'negro', 4: 'timeout' }
const STATUS_LABEL = { 0: '—', 1: 'BLANC', 2: 'GRIS', 3: 'NEGRO', 4: 'TIME' }
const STATUS_CLASS = ['', 'blanco', 'gris', 'negro', 'timeout']

/* ── Bar image overlay ───────────────────────────────────────────────────── */

// Physical sensor positions in the image for each bar type.
// sensorTopPct: vertical % from top where sensors sit in the image.
// leftPct(i, n): horizontal % for sensor array index i (0 = S1, n-1 = S8).
//   For QTR-8RC: S1 is physically on the right, S8 on the left.
function buildSensorGrid(n) {
  sensorCount = n
  els.sensorGrid.innerHTML = ''
  els.sensorGrid.style.flexDirection = n <= 8 ? 'row-reverse' : 'row'
  for (let i = 0; i < n; i++) {
    const card = document.createElement('div')
    card.className = 'sensor-card'
    card.id = `sc-${i}`
    card.innerHTML = `
      <div class="sensor-label">${n <= 8 ? 'S' + (i+1) : 'S' + String(i).padStart(2,'0')}</div>
      <div class="sensor-bar-container">
        <div class="sensor-bar-fill" id="sbf-${i}" style="height:0%"></div>
      </div>
      <div class="sensor-value" id="sv-${i}">—</div>
      <div class="sensor-status" id="ss-${i}">—</div>
    `
    els.sensorGrid.appendChild(card)
  }
}

function updateSensorGrid(event) {
  const { v, s, pos, negro, gris, blanco, tout } = event
  const n = Math.min(v.length, sensorCount)

  for (let i = 0; i < n; i++) {
    const statusName = STATUS[s[i]] || 'unknown'
    const card  = $(`sc-${i}`)
    const fill  = $(`sbf-${i}`)
    const valEl = $(`sv-${i}`)
    const stEl  = $(`ss-${i}`)
    if (!card) continue

    card.className  = 'sensor-card ' + (tout > 0 ? 'timeout' : statusName)
    fill.style.height = (v[i] / 10).toFixed(1) + '%'
    valEl.textContent = v[i]
    stEl.textContent  = STATUS_LABEL[s[i]] || '—'
  }

  // Position
  const range = posMax - posMin
  const frac  = range > 0 ? (pos - posMin) / range : 0.5
  const pct   = Math.max(0, Math.min(100, frac * 100))
  els.posNeedle.style.left   = pct.toFixed(1) + '%'
  els.posValue.textContent   = (pos >= 0 ? '+' : '') + pos

  // Summary
  els.sumNegro.textContent  = negro
  els.sumGris.textContent   = gris
  els.sumBlanco.textContent = blanco
  els.timeoutBadge.style.display = tout > 0 ? 'flex' : 'none'

  lastSensor = event
}

/* ── Diagnostic steps ───────────────────────────────────────────────────── */

function diagStep(n, active, done, failed, warned) {
  const el = document.querySelector(`.diag-step[data-step="${n}"]`)
  if (!el) return
  el.className = 'diag-step ' + (failed ? 'error' : warned ? 'warn' : done ? 'done' : active ? 'active' : 'pending')
  const icon = el.querySelector('.step-status-icon')
  icon.textContent = failed ? '✕' : warned ? '⚠' : done ? '✓' : active ? '⟳' : '○'
}

function resetDiagSteps() {
  for (let i = 1; i <= 5; i++) diagStep(i, false, false, false, false)
}

/* ── Visor events ───────────────────────────────────────────────────────── */

window.visor.onEvent(e => {
  switch (e.t) {

    case 'connected':
      sensorCount = e.sensors
      posMin = e.pos_min
      posMax = e.pos_max
      els.monBarName.textContent = e.bar
      els.monPort.textContent    = e.port
      els.monBaud.textContent    = e.baudrate + ' bd'
      els.posMin.textContent     = e.pos_min
      els.posMax.textContent     = e.pos_max
      buildSensorGrid(e.sensors)
      els.calibDone.style.display = 'none'
      els.calibSpinner.classList.remove('done')
      showPanel('calibrating')
      setState(State.MONITORING)
      break

    case 'calib':
      break

    case 'sensor':
      if (appState === State.MONITORING) {
        const wasCalib = els.stateCalib.style.display !== 'none'
        if (wasCalib) {
          els.calibDone.style.display = 'flex'
          els.calibSpinner.classList.add('done')
          setTimeout(() => showPanel('monitor'), 800)
        } else {
          showPanel('monitor')
        }
        updateSensorGrid(e)
      } else if (appState === State.DIAGNOSTIC) {
        els.diagSensorArea.style.display = 'flex'
        updateSensorGrid(e)
      }
      break

    case 'step':
      diagStep(e.n, true, false, false, false)
      break

    case 'step_done':
      if (!e.ok) {
        diagStep(e.n, false, false, false, true)
      } else {
        diagStep(e.n, false, true, false, false)
      }
      break

    case 'monitoring':
      sensorCount = e.sensors
      posMin = e.pos_min
      posMax = e.pos_max
      els.posMin.textContent = e.pos_min
      els.posMax.textContent = e.pos_max
      buildSensorGrid(e.sensors)
      diagStep(5, true, false, false, false)
      els.diagSensorArea.style.display = 'flex'
      break

    case 'error':
      els.errorMsg.textContent = e.msg || 'Error desconocido'
      showPanel('error')
      setState(State.ERROR)
      break

    case 'done':
      if (appState === State.RESTORING) {
        if (e.ok) {
          showPanel('idle')
        } else {
          els.errorMsg.textContent =
            'No se pudo restaurar el firmware automáticamente.\n\n' +
            'Ejecutar manualmente:\n' + (e.restore_cmd || '')
          showPanel('error')
        }
        setState(State.IDLE)
      }
      break

    case 'stopped':
      showPanel('idle')
      setState(State.IDLE)
      break

    case 'disconnected':
      if (appState !== State.RESTORING && appState !== State.ERROR) {
        showPanel('idle')
        setState(State.IDLE)
      }
      break

    case 'diag_closed':
      if (appState === State.DIAGNOSTIC) {
        setState(State.RESTORING)
        showPanel('restoring')
        window.visor.restore()
      }
      break

    case 'proc_closed':
      break
  }
})

window.visor.onLog(line => {
  const out = els.logOutput
  out.textContent += line + '\n'
  out.scrollTop = out.scrollHeight
})

/* ── Config loading ─────────────────────────────────────────────────────── */

async function loadConfig() {
  const cfg = await window.visor.getConfig()
  if (!cfg) return

  if (cfg.port)            els.selPort.value       = cfg.port
  if (cfg.baudrate)        els.selBaudrate.value   = String(cfg.baudrate)
  if (cfg.board_fqbn)      els.selBoard.value      = cfg.board_fqbn
  if (cfg.bar_type != null) {
    els.selBartype.value = String(cfg.bar_type)
    updateBarImages(String(cfg.bar_type))
  }
  if (cfg.threshold_black) els.inpThrBlack.value   = cfg.threshold_black
  if (cfg.threshold_white) els.inpThrWhite.value   = cfg.threshold_white
  if (cfg.timeout_ms)      els.inpTimeout.value    = cfg.timeout_ms

  // Config display tab
  const keys = ['port','baudrate','bar_type','threshold_black','threshold_white',
                'timeout_ms','board_fqbn','bar_name']
  const labels = {
    port: 'Puerto', baudrate: 'Baudrate', bar_type: 'Tipo barra',
    threshold_black: 'Umbral Negro', threshold_white: 'Umbral Blanco',
    timeout_ms: 'Timeout (ms)', board_fqbn: 'Board FQBN', bar_name: 'Barra',
  }
  els.configDisplay.innerHTML = keys.map(k => {
    const val = cfg[k] != null ? cfg[k] : '—'
    return `<div class="cfg-key">${labels[k]||k}</div><div class="cfg-val">${val}</div>`
  }).join('')
}

async function loadPorts() {
  const ports = await window.visor.listPorts()
  const current = els.selPort.value
  els.selPort.innerHTML = '<option value="">-- Seleccionar --</option>'
  if (ports) {
    ports.forEach(p => {
      const opt = document.createElement('option')
      opt.value = opt.textContent = p
      if (p === current) opt.selected = true
      els.selPort.appendChild(opt)
    })
  }
}

/* ── Save config ────────────────────────────────────────────────────────── */

async function saveAllConfig() {
  const changes = [
    ['port',            els.selPort.value],
    ['baudrate',        els.selBaudrate.value],
    ['board_fqbn',      els.selBoard.value],
    ['bar_type',        els.selBartype.value],
    ['threshold_black', els.inpThrBlack.value],
    ['threshold_white', els.inpThrWhite.value],
    ['timeout_ms',      els.inpTimeout.value],
  ]
  for (const [key, val] of changes) {
    if (val) await window.visor.setConfig(key, val)
  }
}

/* ── Button handlers ────────────────────────────────────────────────────── */

const BAR_IMAGES = {
  '0': 'assets/qtr8rc.jpg',
  '1': null,
}

function updateBarImages(barType) {
  const src = BAR_IMAGES[barType]
  const show = !!src
  ;[els.barThumb, els.idleBarImg, els.calibBarImg].forEach(img => {
    if (!img) return
    img.src = src || ''
    img.style.display = show ? '' : 'none'
    img.parentElement.style.display =
      (img === els.barThumb) ? (show ? '' : 'none') : ''
  })
}

els.selBartype.addEventListener('change', async () => {
  updateBarImages(els.selBartype.value)
  await window.visor.setConfig('bar_type', els.selBartype.value)
  const cfg = await window.visor.getConfig()
  if (!cfg) return
  els.selBaudrate.value  = String(cfg.baudrate)
  els.inpThrBlack.value  = cfg.threshold_black
  els.inpThrWhite.value  = cfg.threshold_white
  els.inpTimeout.value   = cfg.timeout_ms
})

els.btnRefreshPorts.addEventListener('click', loadPorts)

els.btnSave.addEventListener('click', saveAllConfig)

els.btnMonitor.addEventListener('click', async () => {
  if (!els.selPort.value) {
    alert('Selecciona un puerto COM primero.')
    return
  }
  await saveAllConfig()
  frozen = false
  setState(State.MONITORING)
  showPanel('waiting')
  els.waitingMsg.textContent = 'Abriendo puerto serial...'
  window.visor.startMonitor()
})

els.btnDiagnostic.addEventListener('click', async () => {
  if (!els.selPort.value) {
    alert('Selecciona un puerto COM primero.')
    return
  }
  await saveAllConfig()
  resetDiagSteps()
  els.diagSensorArea.style.display = 'none'
  setState(State.DIAGNOSTIC)
  showPanel('diagnostic')
  window.visor.startDiagnostic()
})

els.btnStop.addEventListener('click', () => {
  if (appState === State.DIAGNOSTIC) {
    window.visor.stop()
    setState(State.RESTORING)
    showPanel('restoring')
    window.visor.restore()
  } else {
    window.visor.stop()
    setState(State.IDLE)
    showPanel('idle')
  }
})

els.btnErrorOk.addEventListener('click', () => {
  setState(State.IDLE)
  showPanel('idle')
})

els.btnClearLog.addEventListener('click', () => {
  els.logOutput.textContent = ''
})

/* ── Keyboard: F = freeze ───────────────────────────────────────────────── */

document.addEventListener('keydown', e => {
  if ((e.key === 'f' || e.key === 'F') && appState === State.MONITORING) {
    frozen = !frozen
    els.freezeBadge.style.display = frozen ? 'flex' : 'none'
    if (!frozen && lastSensor) updateSensorGrid(lastSensor)
  }
})

/* ── Init ───────────────────────────────────────────────────────────────── */

;(async () => {
  showPanel('idle')
  setState(State.IDLE)
  await loadPorts()
  await loadConfig()

  // Select the port if already configured
  const cfg = await window.visor.getConfig()
  if (cfg && cfg.port) {
    let found = false
    for (const opt of els.selPort.options) {
      if (opt.value === cfg.port) { opt.selected = true; found = true; break }
    }
    if (!found) {
      const opt = document.createElement('option')
      opt.value = opt.textContent = cfg.port
      opt.selected = true
      els.selPort.appendChild(opt)
    }
  }
})()
