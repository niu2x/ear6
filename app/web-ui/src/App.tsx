import { useState, useRef, useEffect } from 'react'
import {
  Activity,
  Clock3,
  FolderOpen,
  Gamepad2,
  Gauge,
  GitCommitHorizontal,
  Keyboard,
  Maximize2,
  Minimize2,
  Pause,
  Play,
  RefreshCw,
  RotateCcw,
  X,
} from 'lucide-react'
import type { Ear6Module } from './types'
import './App.css'

const SYSTEM_NES = 1
const EMULATION_FPS = 60
const FRAME_DURATION_MS = 1000 / EMULATION_FPS
const MAX_CATCH_UP_STEPS = 3

function formatBuildTime(value: string) {
  const date = new Date(value)
  if (Number.isNaN(date.getTime())) return value
  return new Intl.DateTimeFormat(undefined, {
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
    hour12: false,
    timeZoneName: 'short',
  }).format(date)
}

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
  const [initError, setInitError] = useState(false)
  const [isRunning, setIsRunning] = useState(false)
  const [isFullscreen, setIsFullscreen] = useState(false)
  const [statusText, setStatusText] = useState('Ready')
  const [fps, setFps] = useState(0)
  const [stepLoad, setStepLoad] = useState(0)
  const [stepTime, setStepTime] = useState(0)
  const [romName, setRomName] = useState('')
  const [hasRom, setHasRom] = useState(false)
  const [showHelp, setShowHelp] = useState(false)

  useEffect(() => { runningRef.current = isRunning }, [isRunning])

  useEffect(() => {
    let cancelled = false

    Promise.resolve()
      .then(() => window.createEar6())
      .then(mod => {
        const ctx = mod._ear6_web_create(SYSTEM_NES)
        if (cancelled) {
          if (ctx) mod._ear6_web_destroy(ctx)
          return
        }
        modRef.current = mod
        ctxRef.current = ctx
        setReady(true)
      })
      .catch(() => {
        if (!cancelled) setInitError(true)
      })

    return () => {
      cancelled = true
      if (frameIdRef.current) cancelAnimationFrame(frameIdRef.current)
      const mod = modRef.current
      if (mod && ctxRef.current) mod._ear6_web_destroy(ctxRef.current)
    }
  }, [])

  useEffect(() => {
    const handleFullscreen = () => setIsFullscreen(document.fullscreenElement === screenRef.current)
    document.addEventListener('fullscreenchange', handleFullscreen)
    return () => document.removeEventListener('fullscreenchange', handleFullscreen)
  }, [])

  useEffect(() => {
    if (!showHelp) return
    const closeOnEscape = (event: KeyboardEvent) => {
      if (event.key === 'Escape') setShowHelp(false)
    }
    window.addEventListener('keydown', closeOnEscape)
    return () => window.removeEventListener('keydown', closeOnEscape)
  }, [showHelp])

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

        const screenBox = canvas.parentElement
        if (screenBox) {
          const maxW = screenBox.clientWidth
          const maxH = screenBox.clientHeight
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
    let ptr = 0
    try {
      const bytes = new Uint8Array(await file.arrayBuffer())
      ptr = mod._malloc(bytes.length)
      if (!ptr) throw new Error('Unable to allocate ROM memory')
      mod.HEAPU8.set(bytes, ptr)
      if (mod._ear6_web_load(ctxRef.current, ptr, bytes.length) !== 0) {
        throw new Error('ROM load failed')
      }
      romDataRef.current = bytes
      mod._ear6_web_step(ctxRef.current)
      setHasRom(true)
      setRomName(file.name)
      setIsRunning(true)
      setStatusText('Running')
    } catch {
      setHasRom(false)
      setIsRunning(false)
      setStatusText('Unable to load ROM')
    } finally {
      if (ptr) mod._free(ptr)
      input.value = ''
    }
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
    if (!ptr) {
      setStatusText('Reset failed')
      return
    }
    mod.HEAPU8.set(data, ptr)
    const result = mod._ear6_web_load(ctxRef.current, ptr, data.length)
    mod._free(ptr)
    if (result !== 0) {
      setStatusText('Reset failed')
      return
    }
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

  const statusClass = isRunning ? 'running' : hasRom ? 'paused' : 'idle'
  const loadClass = stepLoad >= 100 ? 'critical' : stepLoad >= 70 ? 'warning' : 'healthy'
  const buildTime = formatBuildTime(__EAR6_BUILD_TIME__)

  if (!ready) {
    return (
      <div className="app">
        <div className="loading-screen">
          <Gamepad2 size={34} strokeWidth={1.6} />
          <strong>EAR6</strong>
          <span className={initError ? 'loading-error' : 'loading-text'}>
            {initError ? 'Runtime unavailable' : 'Initializing core'}
          </span>
          {initError && (
            <button className="icon-text-button" onClick={() => window.location.reload()}>
              <RefreshCw size={17} />
              Retry
            </button>
          )}
        </div>
      </div>
    )
  }

  return (
    <div className="app">
      <header className="topbar">
        <div className="brand">
          <span className="brand-mark" aria-hidden="true"><Gamepad2 size={24} /></span>
          <span className="brand-copy">
            <strong>EAR6</strong>
            <small>NES EMULATOR</small>
          </span>
        </div>
        <div className="session-summary">
          <span className={`status-indicator ${statusClass}`} aria-hidden="true"></span>
          <span className="session-state">{statusText}</span>
          <span className="session-divider" aria-hidden="true"></span>
          <span className="rom-label" title={romName || 'No ROM loaded'}>
            {romName || 'No ROM loaded'}
          </span>
        </div>
      </header>

      <main ref={screenRef} className="screen-wrap">
        <div className="screen-box">
          <canvas ref={canvasRef}></canvas>
          {!hasRom && (
            <div className="standby">
              <span>EAR6 / NES</span>
              <strong>NO CARTRIDGE</strong>
              <i aria-hidden="true"></i>
            </div>
          )}
        </div>
      </main>

      <section className="console-deck" aria-label="Emulator controls and performance">
        <nav className="toolbar" aria-label="Emulator controls">
          <input
            ref={fileInputRef}
            type="file"
            accept=".nes,.rom,.bin"
            onChange={openRom}
            aria-label="Open ROM file"
            hidden
          />
          <button className="control-button open-button" onClick={() => fileInputRef.current?.click()} title="Open ROM">
            <FolderOpen size={18} />
            <span className="button-label">Open ROM</span>
          </button>
          <button className="control-button" onClick={resetRom} disabled={!hasRom} title="Reset">
            <RotateCcw size={18} />
            <span className="button-label">Reset</span>
          </button>
          <button className="control-button primary" onClick={toggleRun} disabled={!hasRom} title={isRunning ? 'Pause' : 'Run'}>
            {isRunning ? <Pause size={18} fill="currentColor" /> : <Play size={18} fill="currentColor" />}
            <span className="button-label">{isRunning ? 'Pause' : 'Run'}</span>
          </button>
          <button className="control-button" onClick={toggleFullscreen} title={isFullscreen ? 'Exit fullscreen' : 'Fullscreen'}>
            {isFullscreen ? <Minimize2 size={18} /> : <Maximize2 size={18} />}
            <span className="button-label">{isFullscreen ? 'Exit' : 'Fullscreen'}</span>
          </button>
          <button className="control-button" onClick={() => setShowHelp(true)} title="Controls">
            <Keyboard size={18} />
            <span className="button-label">Controls</span>
          </button>
        </nav>

        <div className="runtime-stats" aria-label="Runtime performance">
          <div className="runtime-stat status-stat" aria-live="polite">
            <span className="stat-label"><Activity size={14} /> STATUS</span>
            <strong className={statusClass}>{statusText}</strong>
          </div>
          <div className="runtime-stat fps-stat">
            <span className="stat-label">FPS</span>
            <strong>{hasRom ? fps : '--'}</strong>
            <small>/ 60</small>
          </div>
          <div
            className={`runtime-stat step-stat ${loadClass}`}
            title="Average ear6_step time as a share of the 16.67 ms frame budget"
          >
            <span className="stat-label"><Gauge size={14} /> STEP LOAD</span>
            <strong>{hasRom ? stepLoad.toFixed(1) : '--'}%</strong>
            <small>{hasRom ? `${stepTime.toFixed(2)} ms` : '16.67 ms budget'}</small>
            <span
              className="load-meter"
              role="progressbar"
              aria-label="Step load"
              aria-valuemin={0}
              aria-valuemax={100}
              aria-valuenow={Math.min(100, Math.round(stepLoad))}
            >
              <i style={{ width: `${Math.min(100, stepLoad)}%` }}></i>
            </span>
          </div>
        </div>
      </section>

      <footer className="status-bar">
        <span><GitCommitHorizontal size={13} /> REV {__EAR6_GIT_REV__}</span>
        <span title={__EAR6_BUILD_TIME__}>
          <Clock3 size={13} /> BUILT <time dateTime={__EAR6_BUILD_TIME__}>{buildTime}</time>
        </span>
      </footer>

      {showHelp && (
        <div className="modal-backdrop" onClick={() => setShowHelp(false)}>
          <div
            className="modal"
            role="dialog"
            aria-modal="true"
            aria-labelledby="controls-title"
            onClick={(e) => e.stopPropagation()}
          >
            <div className="modal-head">
              <div>
                <span>PLAYER ONE</span>
                <strong id="controls-title">Keyboard controls</strong>
              </div>
              <button className="icon-button" onClick={() => setShowHelp(false)} title="Close controls" aria-label="Close controls">
                <X size={20} />
              </button>
            </div>
            <div className="control-map">
              <div><kbd>ARROWS</kbd><span>D-Pad</span></div>
              <div><kbd>Z</kbd><span>A button</span></div>
              <div><kbd>X</kbd><span>B button</span></div>
              <div><kbd>ENTER</kbd><span>Start</span></div>
              <div><kbd>SHIFT</kbd><span>Select</span></div>
            </div>
          </div>
        </div>
      )}
    </div>
  )
}

export default App
