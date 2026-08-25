import { useState, useRef, useEffect } from 'react'
import type { Ear6Module } from './types'
import './App.css'

const SYSTEM_NES = 1
const EMULATION_FPS = 60
const FRAME_DURATION_MS = 1000 / EMULATION_FPS
const MAX_CATCH_UP_STEPS = 3

function App() {
  const modRef = useRef<Ear6Module | null>(null)
  const ctxRef = useRef(0)
  const runningRef = useRef(false)
  const romDataRef = useRef<Uint8Array | null>(null)
  const frameIdRef = useRef(0)
  const canvasRef = useRef<HTMLCanvasElement | null>(null)
  const screenRef = useRef<HTMLDivElement | null>(null)
  const fileInputRef = useRef<HTMLInputElement | null>(null)

  const [ready, setReady] = useState(false)
  const [isRunning, setIsRunning] = useState(false)
  const [statusText, setStatusText] = useState('Ready')
  const [fps, setFps] = useState(0)
  const [stepLoad, setStepLoad] = useState(0)
  const [stepTime, setStepTime] = useState(0)
  const [romName, setRomName] = useState('')
  const [hasRom, setHasRom] = useState(false)
  const [showHelp, setShowHelp] = useState(false)

  useEffect(() => { runningRef.current = isRunning }, [isRunning])

  useEffect(() => {
    window.createEar6().then(mod => {
      modRef.current = mod
      ctxRef.current = mod._ear6_web_create(SYSTEM_NES)
      setReady(true)
    })

    return () => {
      if (frameIdRef.current) cancelAnimationFrame(frameIdRef.current)
      const mod = modRef.current
      if (mod && ctxRef.current) mod._ear6_web_destroy(ctxRef.current)
    }
  }, [])

  useEffect(() => {
    const keyMap: Record<string, number> = {
      ArrowUp:    4,
      ArrowDown:  5,
      ArrowLeft:  6,
      ArrowRight: 7,
      KeyZ:       1,
      KeyX:       0,
      Enter:      3,
      ShiftLeft:  2,
      ShiftRight: 2,
    }

    const handleKey = (pressed: number) => (e: KeyboardEvent) => {
      const button = keyMap[e.code]
      if (button === undefined) return
      e.preventDefault()
      const mod = modRef.current
      const ctx = ctxRef.current
      if (mod && ctx) {
        mod._ear6_web_nes_set_button_state(ctx, button, pressed)
      }
    }

    const onDown = handleKey(1)
    const onUp = handleKey(0)
    window.addEventListener('keydown', onDown)
    window.addEventListener('keyup', onUp)
    return () => {
      window.removeEventListener('keydown', onDown)
      window.removeEventListener('keyup', onUp)
    }
  }, [])

  useEffect(() => {
    const mod = modRef.current
    if (!mod || !ctxRef.current || !canvasRef.current) return
    const canvas = canvasRef.current
    let lastAnimationTime: number | null = null
    let accumulatedTime = 0
    let fpsWindowStart = performance.now()
    let emulatedFrames = 0
    let totalStepTime = 0

    const draw = (time: number) => {
      if (lastAnimationTime === null) lastAnimationTime = time
      const elapsed = time - lastAnimationTime
      lastAnimationTime = time

      if (runningRef.current) {
        // requestAnimationFrame follows the display refresh rate, which may be
        // 120Hz or higher. Keep emulation time independent from repaint rate.
        accumulatedTime += elapsed
        let steps = 0
        while (accumulatedTime >= FRAME_DURATION_MS && steps < MAX_CATCH_UP_STEPS) {
          const stepStart = performance.now()
          mod._ear6_web_step(ctxRef.current)
          totalStepTime += performance.now() - stepStart
          accumulatedTime -= FRAME_DURATION_MS
          emulatedFrames += 1
          steps += 1
        }
        if (steps === MAX_CATCH_UP_STEPS) {
          accumulatedTime %= FRAME_DURATION_MS
        }
      } else {
        accumulatedTime = 0
      }

      const ptr = mod._ear6_web_get_framebuffer(ctxRef.current)
      const w = mod._ear6_web_get_frame_width(ctxRef.current)
      const h = mod._ear6_web_get_frame_height(ctxRef.current)
      if (ptr && w > 0 && h > 0) {
        if (canvas.width !== w || canvas.height !== h) {
          canvas.width = w
          canvas.height = h
        }
        const c2d = canvas.getContext('2d')
        if (c2d) {
          const imageData = c2d.createImageData(w, h)
          const src = new Uint8Array(mod.HEAPU8.buffer, ptr, w * h * 4)
          imageData.data.set(src)
          c2d.putImageData(imageData, 0, 0)
        }

        if (screenRef.current) {
          const maxW = screenRef.current.clientWidth
          const maxH = screenRef.current.clientHeight
          const scale = Math.min(maxW / w, maxH / h)
          canvas.style.width = `${Math.floor(w * scale)}px`
          canvas.style.height = `${Math.floor(h * scale)}px`
        }
      }

      const fpsElapsed = time - fpsWindowStart
      if (fpsElapsed >= 1000) {
        const averageStepTime = emulatedFrames > 0 ? totalStepTime / emulatedFrames : 0
        setFps(Math.round(emulatedFrames * 1000 / fpsElapsed))
        setStepTime(averageStepTime)
        setStepLoad(averageStepTime / FRAME_DURATION_MS * 100)
        emulatedFrames = 0
        totalStepTime = 0
        fpsWindowStart = time
      }
      frameIdRef.current = requestAnimationFrame(draw)
    }
    frameIdRef.current = requestAnimationFrame(draw)
  }, [ready])

  const openRom = async () => {
    const mod = modRef.current
    const input = fileInputRef.current
    if (!mod || !input || !ctxRef.current) return
    const file = input.files?.[0]
    if (!file) return
    setStatusText('Loading ROM...')
    const bytes = new Uint8Array(await file.arrayBuffer())
    romDataRef.current = bytes
    const ptr = mod._malloc(bytes.length)
    mod.HEAPU8.set(bytes, ptr)
    mod._ear6_web_load(ctxRef.current, ptr, bytes.length)
    mod._free(ptr)
    mod._ear6_web_step(ctxRef.current)
    setHasRom(true)
    setRomName(file.name)
    setIsRunning(true)
    setStatusText('Running')
  }

  const toggleRun = () => {
    if (!hasRom) return
    const next = !isRunning
    setIsRunning(next)
    setStatusText(next ? 'Running' : 'Paused')
  }

  const resetRom = () => {
    const mod = modRef.current
    const data = romDataRef.current
    if (!mod || !data || !ctxRef.current) return
    const ptr = mod._malloc(data.length)
    mod.HEAPU8.set(data, ptr)
    mod._ear6_web_load(ctxRef.current, ptr, data.length)
    mod._free(ptr)
    mod._ear6_web_step(ctxRef.current)
    setIsRunning(false)
    setStatusText('Reset complete')
  }

  const toggleFullscreen = () => {
    if (!screenRef.current) return
    if (!document.fullscreenElement) {
      screenRef.current.requestFullscreen().catch(() => setStatusText('Fullscreen blocked'))
    } else {
      document.exitFullscreen()
    }
  }

  if (!ready) {
    return (
      <div className="app">
        <div className="loading-screen">
          <div className="loading-text">Initializing Ear6...</div>
        </div>
      </div>
    )
  }

  return (
    <div className="app">
      <header className="topbar">
        <div className="brand">
          <span className={`dot ${isRunning ? 'running' : hasRom ? 'paused' : ''}`}></span>
          <strong>Ear6 Emulator</strong>
        </div>
        <span className="rom-label">{romName || 'No ROM loaded'}</span>
      </header>

      <section className="toolbar">
        <input ref={fileInputRef} type="file" accept=".nes,.rom,.bin" onChange={openRom} hidden />
        <button onClick={() => fileInputRef.current?.click()}>Open ROM</button>
        <button onClick={resetRom} disabled={!hasRom}>Reset</button>
        <button className="primary" onClick={toggleRun} disabled={!hasRom}>{isRunning ? 'Pause' : 'Run'}</button>
        <button className="secondary" onClick={toggleFullscreen}>Fullscreen</button>
        <button onClick={() => setShowHelp(true)}>How to Play</button>
      </section>

      <main ref={screenRef} className="screen-wrap">
        <div className="screen-box">
          <canvas ref={canvasRef}></canvas>
          {!hasRom && <div className="standby">LOAD A ROM TO START</div>}
        </div>
      </main>

      <footer className="status-bar">
        <span>Status: {statusText}</span>
        <span>FPS: {fps}</span>
        <span title="Average ear6_step time as a share of the 16.67 ms frame budget">
          Step: {stepLoad.toFixed(1)}% ({stepTime.toFixed(2)} ms)
        </span>
        <span title={__EAR6_BUILD_TIME__}>
          Rev: {__EAR6_GIT_REV__} · Built: {__EAR6_BUILD_TIME__.slice(0, 16).replace('T', ' ')} UTC
        </span>
      </footer>

      {showHelp && (
        <div className="modal-backdrop" onClick={() => setShowHelp(false)}>
          <div className="modal" onClick={(e) => e.stopPropagation()}>
            <div className="modal-head">
              <strong>Controls</strong>
              <button onClick={() => setShowHelp(false)}>Close</button>
            </div>
            <p>Arrow keys: D-Pad, Z: A, X: B, Enter: Start, Shift: Select.</p>
          </div>
        </div>
      )}
    </div>
  )
}

export default App
