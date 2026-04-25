import type { SliderWebSocket } from './websocket'

function el<T extends HTMLElement>(id: string): T {
  return document.getElementById(id) as T
}

export function initUI(ws: SliderWebSocket) {
  const statusDot     = el('status-dot')
  const statusText    = el('status-text')
  const statusLine    = el('status-line')
  const sliderDisplay = el<HTMLSpanElement>('pos-slider')
  const panDisplay    = el<HTMLSpanElement>('pos-pan')
  const tiltDisplay   = el<HTMLSpanElement>('pos-tilt')

  const sliderCtrl    = el<HTMLInputElement>('ctrl-slider')
  const panCtrl       = el<HTMLInputElement>('ctrl-pan')
  const tiltCtrl      = el<HTMLInputElement>('ctrl-tilt')
  const speedCtrl     = el<HTMLInputElement>('ctrl-speed')
  const accelCtrl     = el<HTMLInputElement>('ctrl-accel')
  const sliderVal     = el<HTMLSpanElement>('val-slider')
  const panVal        = el<HTMLSpanElement>('val-pan')
  const tiltVal       = el<HTMLSpanElement>('val-tilt')
  const speedVal      = el<HTMLSpanElement>('val-speed')
  const accelVal      = el<HTMLSpanElement>('val-accel')

  const btnStop       = el<HTMLButtonElement>('btn-stop')
  const btnSetPos     = el<HTMLButtonElement>('btn-set-pos')
  const btnCalPan     = el<HTMLButtonElement>('btn-cal-pan')
  const btnCalTilt    = el<HTMLButtonElement>('btn-cal-tilt')
  const btnCalSlide   = el<HTMLButtonElement>('btn-cal-slide')
  const btnSave       = el<HTMLButtonElement>('btn-save')

  const limitsToggle  = el<HTMLButtonElement>('limits-toggle')
  const limitsPanel   = el('limits-panel')
  const limitsContent = el('limits-content')

  // --- Connection state ---
  ws.onConnection((connected) => {
    statusDot.className = connected ? 'dot connected' : 'dot disconnected'
    statusText.textContent = connected ? 'Connected' : 'Disconnected'
    if (connected) {
      statusLine.textContent = `ws://${window.location.hostname}/ws`
    } else {
      statusLine.textContent = 'Reconnecting...'
    }
    setControlsEnabled(connected)
  })

  // --- Live positions ---
  ws.onMessage((msg) => {
    if (msg.type === 'position') {
      sliderDisplay.textContent = formatFrac(msg.slider)
      panDisplay.textContent    = formatFrac(msg.pan)
      tiltDisplay.textContent   = formatFrac(msg.tilt)
    } else if (msg.type === 'status') {
      statusLine.textContent = msg.value
    }
  })

  // --- Position control live value display ---
  sliderCtrl.addEventListener('input', () => { sliderVal.textContent = Number(sliderCtrl.value).toFixed(3) })
  panCtrl.addEventListener('input',    () => { panVal.textContent    = Number(panCtrl.value).toFixed(3) })
  tiltCtrl.addEventListener('input',   () => { tiltVal.textContent   = Number(tiltCtrl.value).toFixed(3) })

  // --- Speed / Accel live update ---
  speedCtrl.addEventListener('input', () => {
    const v = Number(speedCtrl.value)
    speedVal.textContent = v.toFixed(2)
    ws.sendCommandNoReply(`set_speed ${v.toFixed(3)}`)
  })

  accelCtrl.addEventListener('input', () => {
    const v = Number(accelCtrl.value)
    accelVal.textContent = v.toFixed(2)
    ws.sendCommandNoReply(`set_accel ${v.toFixed(3)}`)
  })

  // --- Control buttons ---
  btnStop.addEventListener('click', () => ws.sendCommandNoReply('stop'))

  btnSetPos.addEventListener('click', () => {
    const s = Number(sliderCtrl.value).toFixed(3)
    const p = Number(panCtrl.value).toFixed(3)
    const t = Number(tiltCtrl.value).toFixed(3)
    ws.sendCommandNoReply(`set_pos ${s} ${p} ${t}`)
  })

  btnCalPan.addEventListener('click', async () => {
    btnCalPan.disabled = true
    btnCalPan.textContent = 'Calibrating...'
    await ws.sendCommand('calibrate pan')
    btnCalPan.disabled = false
    btnCalPan.textContent = 'Calibrate Pan'
  })

  btnCalTilt.addEventListener('click', async () => {
    btnCalTilt.disabled = true
    btnCalTilt.textContent = 'Calibrating...'
    await ws.sendCommand('calibrate tilt')
    btnCalTilt.disabled = false
    btnCalTilt.textContent = 'Calibrate Tilt'
  })

  btnCalSlide.addEventListener('click', async () => {
    btnCalSlide.disabled = true
    btnCalSlide.textContent = 'Calibrating...'
    await ws.sendCommand('calibrate slide')
    btnCalSlide.disabled = false
    btnCalSlide.textContent = 'Calibrate Slide'
  })

  btnSave.addEventListener('click', async () => {
    btnSave.disabled = true
    btnSave.textContent = 'Saving...'
    await ws.sendCommand('save')
    btnSave.disabled = false
    btnSave.textContent = 'Save Config'
  })

  // --- Motor limits panel ---
  limitsToggle.addEventListener('click', async () => {
    const isOpen = limitsPanel.classList.toggle('open')
    limitsToggle.textContent = isOpen ? 'Motor Limits ▲' : 'Motor Limits ▼'
    if (isOpen) await loadLimits()
  })

  async function loadLimits() {
    limitsContent.textContent = 'Loading...'

    const [panResp, tiltResp, slideResp, calResp] = await Promise.all([
      ws.sendCommand('get_motor_pan_limits'),
      ws.sendCommand('get_motor_tilt_limits'),
      ws.sendCommand('get_motor_slide_limits'),
      ws.sendCommand('get_slider_calibration'),
    ])

    limitsContent.innerHTML = `
      <pre>${[panResp, tiltResp, slideResp, calResp].join('\n')}</pre>
      <p class="limits-hint">Use the serial monitor or extend this panel to edit limits.</p>
    `
  }

  function setControlsEnabled(enabled: boolean) {
    const controls = [btnStop, btnSetPos, btnCalPan, btnCalTilt, btnCalSlide,
                      btnSave, speedCtrl, accelCtrl, sliderCtrl, panCtrl, tiltCtrl]
    controls.forEach(c => (c.disabled = !enabled))
  }

  // Start disabled until connected
  setControlsEnabled(false)
}

function formatFrac(v: number): string {
  return (v >= 0 ? '+' : '') + v.toFixed(3)
}
