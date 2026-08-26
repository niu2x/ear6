import { useState, useRef, useEffect } from 'react'
import {
  Activity,
  ArchiveRestore,
  ChevronDown,
  Clock3,
  Download,
  FileUp,
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
  Save,
  Trash2,
  X,
} from 'lucide-react'
import type { Ear6Module } from './types'
import {
  decodeState,
  encodeState,
  listStoredSaves,
  removeStoredSave,
  writeStoredSave,
  type StoredSave,
} from './saveStore'
import { NesWebAudioPlayer } from './webAudio'
import './App.css'

const SYSTEM_NES = 1
const EMULATION_FPS = 60
const FRAME_DURATION_MS = 1000 / EMULATION_FPS
const MAX_CATCH_UP_STEPS = 3

function drainAudio(mod: Ear6Module, ctx: number, player: NesWebAudioPlayer | null) {
  let frameCount = mod._ear6_web_get_audio_num_samples(ctx)
  while (frameCount > 0) {
    const ptr = mod._ear6_web_get_audiobuffer(ctx)
    if (ptr && player) {
      const samples = new Int16Array(mod.HEAPU8.buffer, ptr, frameCount * 2)
      player.enqueueStereoS16(samples, frameCount)
    }
    mod._ear6_web_consume_audio(ctx)
    frameCount = mod._ear6_web_get_audio_num_samples(ctx)
  }
}

function allocateCString(mod: Ear6Module, value: string) {
  const encoded = new TextEncoder().encode(value)
  const ptr = mod._malloc(encoded.length + 1)
  if (!ptr) return 0
  mod.HEAPU8.set(encoded, ptr)
  mod.HEAPU8[ptr + encoded.length] = 0
  return ptr
}

function stateFileStem(name: string) {
  const stem = /\.(?:e6s|ear6state)$/i.test(name)
    ? name.replace(/\.(?:e6s|ear6state)$/i, '')
    : name.replace(/\.[^.]+$/, '')
  return stem || 'ear6-session'
}

function formatSaveTime(value: string) {
  const date = new Date(value)
  if (Number.isNaN(date.getTime())) return value
  return new Intl.DateTimeFormat(undefined, {
    dateStyle: 'medium',
    timeStyle: 'short',
  }).format(date)
}

interface CapturedState {
  bytes: Uint8Array
  systemType: number
  contentIdentity: string
}

function readStateIdentity(mod: Ear6Module, statePtr: number, stateSize: number) {
  const infoPtr = mod._malloc(12)
  if (!infoPtr) throw new Error('Unable to allocate state metadata')
  try {
    if (mod._ear6_web_get_state_identity(
      statePtr,
      stateSize,
      infoPtr,
      infoPtr + 4,
      infoPtr + 8,
    ) !== 0) {
      throw new Error('Invalid state metadata')
    }
    const view = new DataView(mod.HEAPU8.buffer)
    const systemType = view.getUint32(infoPtr, true)
    const low = BigInt(view.getUint32(infoPtr + 4, true))
    const high = BigInt(view.getUint32(infoPtr + 8, true))
    const contentIdentity = ((high << 32n) | low).toString(16).padStart(16, '0')
    return { systemType, contentIdentity }
  } finally {
    mod._free(infoPtr)
  }
}

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
  const stateInputRef = useRef<HTMLInputElement | null>(null)
  const saveMenuRef = useRef<HTMLDivElement | null>(null)
  const audioPlayerRef = useRef<NesWebAudioPlayer | null>(null)

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
  const [canReset, setCanReset] = useState(false)
  const [showHelp, setShowHelp] = useState(false)
  const [showSaveMenu, setShowSaveMenu] = useState(false)
  const [storedSaves, setStoredSaves] = useState<StoredSave[]>([])

  const unlockAudio = () => {
    if (!audioPlayerRef.current) {
      audioPlayerRef.current = new NesWebAudioPlayer()
    }
    audioPlayerRef.current.unlock()
  }

  const refreshStoredSaves = () => {
    try {
      setStoredSaves(listStoredSaves().filter(save => save.systemType === SYSTEM_NES))
      return true
    } catch {
      setStoredSaves([])
      return false
    }
  }

  useEffect(() => { runningRef.current = isRunning }, [isRunning])

  useEffect(() => {
    refreshStoredSaves()
    const refresh = () => refreshStoredSaves()
    window.addEventListener('storage', refresh)
    return () => window.removeEventListener('storage', refresh)
  }, [])

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
      audioPlayerRef.current?.close()
      audioPlayerRef.current = null
    }
  }, [])

  useEffect(() => {
    const handleFullscreen = () => setIsFullscreen(document.fullscreenElement === screenRef.current)
    document.addEventListener('fullscreenchange', handleFullscreen)
    return () => document.removeEventListener('fullscreenchange', handleFullscreen)
  }, [])

  useEffect(() => {
    if (!showHelp && !showSaveMenu) return
    const closeOnEscape = (event: KeyboardEvent) => {
      if (event.key === 'Escape') {
        setShowHelp(false)
        setShowSaveMenu(false)
      }
    }
    window.addEventListener('keydown', closeOnEscape)
    return () => window.removeEventListener('keydown', closeOnEscape)
  }, [showHelp, showSaveMenu])

  useEffect(() => {
    if (!showSaveMenu) return
    const closeOnOutsideClick = (event: PointerEvent) => {
      if (!saveMenuRef.current?.contains(event.target as Node)) {
        setShowSaveMenu(false)
      }
    }
    window.addEventListener('pointerdown', closeOnOutsideClick)
    return () => window.removeEventListener('pointerdown', closeOnOutsideClick)
  }, [showSaveMenu])

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
      if (pressed) unlockAudio()
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
          drainAudio(mod, ctxRef.current, audioPlayerRef.current)
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
    unlockAudio()
    runningRef.current = false
    audioPlayerRef.current?.reset()
    setStatusText('Loading ROM...')
    let ptr = 0
    let namePtr = 0
    try {
      const bytes = new Uint8Array(await file.arrayBuffer())
      ptr = mod._malloc(bytes.length)
      if (!ptr) throw new Error('Unable to allocate ROM memory')
      namePtr = allocateCString(mod, file.name)
      if (!namePtr) throw new Error('Unable to allocate ROM name')
      mod.HEAPU8.set(bytes, ptr)
      if (mod._ear6_web_load_from_memory(ctxRef.current, ptr, bytes.length, namePtr) !== 0) {
        throw new Error('ROM load failed')
      }
      romDataRef.current = bytes
      mod._ear6_web_step(ctxRef.current)
      drainAudio(mod, ctxRef.current, audioPlayerRef.current)
      setHasRom(true)
      setCanReset(true)
      setRomName(file.name)
      runningRef.current = true
      setIsRunning(true)
      setStatusText('Running')
    } catch {
      setHasRom(false)
      setCanReset(false)
      runningRef.current = false
      setIsRunning(false)
      setStatusText('Unable to load ROM')
    } finally {
      if (ptr) mod._free(ptr)
      if (namePtr) mod._free(namePtr)
      input.value = ''
    }
  }

  const captureState = (): CapturedState => {
    const mod = modRef.current
    const ctx = ctxRef.current
    if (!mod || !ctx || !hasRom) throw new Error('No content loaded')

    let sizePtr = 0
    let statePtr = 0
    try {
      sizePtr = mod._malloc(4)
      if (!sizePtr) throw new Error('Unable to allocate state size')
      if (mod._ear6_web_save_state_to_memory(ctx, 0, 0, sizePtr) !== 0) {
        throw new Error('State size query failed')
      }
      const stateSize = new DataView(mod.HEAPU8.buffer).getUint32(sizePtr, true)
      if (!stateSize) throw new Error('Empty state')
      statePtr = mod._malloc(stateSize)
      if (!statePtr) throw new Error('Unable to allocate state memory')
      if (mod._ear6_web_save_state_to_memory(ctx, statePtr, stateSize, sizePtr) !== 0) {
        throw new Error('State save failed')
      }

      const stateBytes = new Uint8Array(stateSize)
      stateBytes.set(new Uint8Array(mod.HEAPU8.buffer, statePtr, stateSize))
      const identity = readStateIdentity(mod, statePtr, stateSize)
      return { bytes: stateBytes, ...identity }
    } finally {
      if (statePtr) mod._free(statePtr)
      if (sizePtr) mod._free(sizePtr)
    }
  }

  const saveLocalState = () => {
    if (!hasRom) return
    try {
      const state = captureState()
      let previewDataUrl: string | null = null
      try {
        previewDataUrl = canvasRef.current?.toDataURL('image/png') ?? null
      } catch {
        previewDataUrl = null
      }
      writeStoredSave({
        version: 1,
        systemType: state.systemType,
        contentIdentity: state.contentIdentity,
        contentName: romName || 'Untitled',
        savedAt: new Date().toISOString(),
        previewDataUrl,
        stateBase64: encodeState(state.bytes),
      })
      refreshStoredSaves()
      setStatusText('Saved locally')
    } catch {
      setStatusText('Unable to save locally')
    }
  }

  const downloadState = () => {
    try {
      const state = captureState()
      const url = URL.createObjectURL(new Blob(
        [state.bytes.buffer],
        { type: 'application/octet-stream' },
      ))
      const anchor = document.createElement('a')
      anchor.href = url
      anchor.download = `${stateFileStem(romName)}.e6s`
      document.body.appendChild(anchor)
      anchor.click()
      anchor.remove()
      URL.revokeObjectURL(url)
      setStatusText('State downloaded')
    } catch {
      setStatusText('Unable to save state')
    }
  }

  const loadStateBytes = (bytes: Uint8Array, contentName: string, successText: string) => {
    const mod = modRef.current
    const ctx = ctxRef.current
    if (!mod || !ctx || bytes.length === 0) return false

    const wasRunning = runningRef.current
    let ptr = 0
    try {
      ptr = mod._malloc(bytes.length)
      if (!ptr) throw new Error('Unable to allocate state memory')
      mod.HEAPU8.set(bytes, ptr)
      const identity = readStateIdentity(mod, ptr, bytes.length)
      if (identity.systemType !== SYSTEM_NES) throw new Error('Unsupported system')
      runningRef.current = false
      if (mod._ear6_web_load_state_from_memory(ctx, ptr, bytes.length) !== 0) {
        throw new Error('State load failed')
      }

      audioPlayerRef.current?.reset()
      romDataRef.current = null
      setHasRom(true)
      setCanReset(false)
      setRomName(contentName)
      setIsRunning(false)
      setFps(0)
      setStepLoad(0)
      setStepTime(0)
      setStatusText(successText)
      return true
    } catch {
      runningRef.current = wasRunning
      setStatusText('Unable to load state')
      return false
    } finally {
      if (ptr) mod._free(ptr)
    }
  }

  const openState = async () => {
    const input = stateInputRef.current
    if (!input) return
    const file = input.files?.[0]
    if (!file) return

    setStatusText('Loading state...')
    try {
      const bytes = new Uint8Array(await file.arrayBuffer())
      loadStateBytes(bytes, stateFileStem(file.name), 'State imported')
    } catch {
      setStatusText('Unable to load state')
    } finally {
      input.value = ''
    }
  }

  const loadStoredState = (save: StoredSave) => {
    try {
      const loaded = loadStateBytes(
        decodeState(save.stateBase64),
        save.contentName,
        'Save loaded',
      )
      if (loaded) setShowSaveMenu(false)
    } catch {
      setStatusText('Unable to load save')
    }
  }

  const deleteStoredState = (save: StoredSave) => {
    try {
      removeStoredSave(save)
      refreshStoredSaves()
      setStatusText('Save deleted')
    } catch {
      setStatusText('Unable to delete save')
    }
  }

  const toggleRun = () => {
    if (!hasRom) return
    const next = !isRunning
    runningRef.current = next
    if (next) {
      unlockAudio()
    } else {
      audioPlayerRef.current?.reset()
    }
    setIsRunning(next)
    setStatusText(next ? 'Running' : 'Paused')
  }

  const resetRom = () => {
    const mod = modRef.current
    const data = romDataRef.current
    if (!mod || !data || !ctxRef.current) return
    const ptr = mod._malloc(data.length)
    const namePtr = allocateCString(mod, romName)
    if (!ptr || !namePtr) {
      if (ptr) mod._free(ptr)
      if (namePtr) mod._free(namePtr)
      setStatusText('Reset failed')
      return
    }
    mod.HEAPU8.set(data, ptr)
    const result = mod._ear6_web_load_from_memory(
      ctxRef.current,
      ptr,
      data.length,
      namePtr,
    )
    mod._free(ptr)
    mod._free(namePtr)
    if (result !== 0) {
      setStatusText('Reset failed')
      return
    }
    runningRef.current = false
    audioPlayerRef.current?.reset()
    mod._ear6_web_step(ctxRef.current)
    drainAudio(mod, ctxRef.current, null)
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
            accept=".nes,.rom,.bin,application/octet-stream"
            onChange={openRom}
            aria-label="Open ROM file"
            hidden
          />
          <button
            className="control-button open-button"
            onClick={() => {
              unlockAudio()
              fileInputRef.current?.click()
            }}
            title="Open ROM"
          >
            <FolderOpen size={18} />
            <span className="button-label">Open ROM</span>
          </button>
          <input
            ref={stateInputRef}
            type="file"
            accept=".e6s,.ear6state,application/octet-stream"
            onChange={openState}
            aria-label="Open state file"
            hidden
          />
          <div className="state-actions" role="group" aria-label="Save state controls">
            <button className="control-button" onClick={saveLocalState} disabled={!hasRom} title="Save locally">
              <Save size={18} />
              <span className="button-label">Save</span>
            </button>
            <div className="save-menu-anchor" ref={saveMenuRef}>
              <button
                className="control-button"
                onClick={() => {
                  if (!showSaveMenu) refreshStoredSaves()
                  setShowSaveMenu(!showSaveMenu)
                }}
                title="Load save"
                aria-haspopup="menu"
                aria-expanded={showSaveMenu}
              >
                <ArchiveRestore size={18} />
                <span className="button-label">Load</span>
                <ChevronDown className="menu-chevron" size={14} />
              </button>
              {showSaveMenu && (
                <div className="save-menu" role="menu" aria-label="Saved games">
                  <div className="save-menu-head">
                    <strong>Load save</strong>
                    <span>{storedSaves.length}</span>
                  </div>
                  <div className="save-menu-list">
                    {storedSaves.length === 0 ? (
                      <div className="save-menu-empty">
                        <ArchiveRestore size={24} />
                        <span>No local saves</span>
                      </div>
                    ) : storedSaves.map(save => (
                      <div className="save-entry" key={`${save.systemType}-${save.contentIdentity}`}>
                        <button
                          className="save-entry-main"
                          role="menuitem"
                          onClick={() => loadStoredState(save)}
                          title={`Load ${save.contentName}`}
                        >
                          <span className="save-preview">
                            {save.previewDataUrl
                              ? <img src={save.previewDataUrl} alt="" />
                              : <Gamepad2 size={22} />}
                          </span>
                          <span className="save-entry-copy">
                            <strong>{save.contentName}</strong>
                            <small>{formatSaveTime(save.savedAt)}</small>
                          </span>
                        </button>
                        <button
                          className="save-delete"
                          onClick={() => deleteStoredState(save)}
                          title={`Delete save for ${save.contentName}`}
                          aria-label={`Delete save for ${save.contentName}`}
                        >
                          <Trash2 size={16} />
                        </button>
                      </div>
                    ))}
                  </div>
                  <div className="save-menu-tools">
                    <button
                      className="icon-text-button"
                      onClick={() => {
                        setShowSaveMenu(false)
                        stateInputRef.current?.click()
                      }}
                    >
                      <FileUp size={16} /> Import .e6s
                    </button>
                    <button
                      className="icon-text-button"
                      onClick={downloadState}
                      disabled={!hasRom}
                    >
                      <Download size={16} /> Export .e6s
                    </button>
                  </div>
                </div>
              )}
            </div>
          </div>
          <button
            className="control-button"
            onClick={resetRom}
            disabled={!canReset}
            title={canReset ? 'Reset' : 'Reset requires an opened ROM'}
          >
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
            <span className={`status-value ${statusClass}`}>{statusText}</span>
          </div>
          <div
            className={`runtime-stat performance-stat ${loadClass}`}
            title="Average ear6_step time as a share of the 16.67 ms frame budget"
          >
            <span className="stat-icon" role="img" aria-label="Performance">
              <Gauge size={18} />
            </span>
            <div className="performance-readings">
              <span className="performance-reading">
                <small>FPS</small>
                <span className="metric-value">{hasRom ? fps : '--'}</span>
                <span className="metric-detail">/ 60</span>
              </span>
              <span className="performance-reading load-reading">
                <small>STEP LOAD</small>
                <span className="metric-value">{hasRom ? stepLoad.toFixed(1) : '--'}%</span>
                <span className="metric-detail">
                  {hasRom ? `${stepTime.toFixed(2)} ms` : '16.67 ms budget'}
                </span>
              </span>
            </div>
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
