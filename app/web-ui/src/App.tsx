import { useState, useRef, useEffect, useCallback } from 'react'
import { TitleBar } from './TitleBar'
import { TabBar } from './TabBar'
import { CrtScreen } from './CrtScreen'
import { ControlBar } from './ControlBar'
import { StatusBar } from './StatusBar'
import type { SystemType, Ear6Module } from './types'
import { SYSTEM_IDS } from './types'
import './App.css'

interface SystemContext {
  ctx: number
  romPtr: number
  romSize: number
  romName: string
}

function createSystemContexts(mod: Ear6Module): Record<SystemType, SystemContext> {
  const create = (sys: SystemType): SystemContext => {
    try {
      const ctx = mod._ear6_web_create(SYSTEM_IDS[sys])
      return { ctx, romPtr: 0, romSize: 0, romName: '' }
    } catch {
      return { ctx: 0, romPtr: 0, romSize: 0, romName: '' }
    }
  }
  return {
    nes: create('nes'),
    test: create('test'),
    flash: create('flash'),
  }
}

function App() {
  const modRef = useRef<Ear6Module | null>(null)
  const contextsRef = useRef<Record<SystemType, SystemContext> | null>(null)
  const isRunningRef = useRef(false)
  const activeCtxRef = useRef(0)

  const [ready, setReady] = useState(false)
  const [activeSystem, setActiveSystem] = useState<SystemType>('nes')
  const [isRunning, setIsRunning] = useState(false)
  const [statusText, setStatusText] = useState('就绪')
  const [fps, setFps] = useState(0)
  const [romName, setRomName] = useState('')
  const [hasRom, setHasRom] = useState(false)
  const [isTransitioning, setIsTransitioning] = useState(false)
  const [flashAnim, setFlashAnim] = useState(false)
  const [wakeAnim, setWakeAnim] = useState(false)

  useEffect(() => { isRunningRef.current = isRunning }, [isRunning])

  useEffect(() => {
    window.createEar6().then(mod => {
      modRef.current = mod
      contextsRef.current = createSystemContexts(mod)
      activeCtxRef.current = contextsRef.current.nes.ctx
      setReady(true)
    })
  }, [])

  useEffect(() => {
    if (contextsRef.current) {
      activeCtxRef.current = contextsRef.current[activeSystem].ctx
    }
  }, [activeSystem])

  useEffect(() => {
    const KEY_MAP: Record<string, number> = {
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
      const button = KEY_MAP[e.code]
      if (button === undefined) return
      e.preventDefault()
      const mod = modRef.current
      const ctx = contextsRef.current?.nes.ctx
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

  const handleOpenRom = useCallback(async () => {
    const mod = modRef.current
    if (!mod) return

    const input = document.createElement('input')
    input.type = 'file'
    input.accept = '.nes,.rom,.bin'
    input.onchange = async () => {
      const file = input.files?.[0]
      if (!file) return
      setStatusText('加载中...')

      const data = await file.arrayBuffer()
      const bytes = new Uint8Array(data)
      const ptr = mod._malloc(bytes.length)
      mod.HEAPU8.set(bytes, ptr)

      const sys = contextsRef.current![activeSystem]
      if (sys.romPtr) mod._free(sys.romPtr)
      sys.romPtr = ptr
      sys.romSize = bytes.length
      sys.romName = file.name

      mod._ear6_web_load(sys.ctx, ptr, bytes.length)
      mod._ear6_web_step(sys.ctx)

      setRomName(file.name)
      setHasRom(true)
      setIsRunning(false)
      setStatusText('就绪')
    }
    input.click()
  }, [activeSystem])

  const handleReset = useCallback(() => {
    const mod = modRef.current
    const sys = contextsRef.current![activeSystem]
    if (!mod || !sys.romPtr) return

    setIsRunning(false)
    setStatusText('已重置')
    mod._ear6_web_load(sys.ctx, sys.romPtr, sys.romSize)
    mod._ear6_web_step(sys.ctx)

    setFlashAnim(true)
    setTimeout(() => setFlashAnim(false), 150)
  }, [activeSystem])

  const handleTogglePlay = useCallback(() => {
    const sys = contextsRef.current![activeSystem]
    if (!modRef.current || !sys.romPtr) return

    const nextRunning = !isRunning
    setIsRunning(nextRunning)
    setStatusText(nextRunning ? '运行中' : '已暂停')

    if (nextRunning) {
      setWakeAnim(true)
      setTimeout(() => setWakeAnim(false), 300)
    }
  }, [activeSystem, isRunning])

  const handleTabChange = useCallback((system: SystemType) => {
    if (system === activeSystem || isTransitioning) return

    setIsTransitioning(true)
    setTimeout(() => {
      setActiveSystem(system)
      const sys = contextsRef.current![system]
      setRomName(sys.romName ?? '')
      setHasRom(!!sys.romPtr)
      setIsRunning(false)
      setStatusText(sys.romPtr ? '已暂停' : '就绪')

      requestAnimationFrame(() => {
        setIsTransitioning(false)
      })
    }, 200)
  }, [activeSystem, isTransitioning])

  const handleRegionChange = useCallback((region: number) => {
    const mod = modRef.current
    const sys = contextsRef.current!.nes
    if (mod && sys.ctx) {
      mod._ear6_web_nes_set_region(sys.ctx, region)
    }
  }, [])

  const titleStatus: 'idle' | 'running' | 'paused' = isRunning
    ? 'running'
    : hasRom
      ? 'paused'
      : 'idle'

  if (!ready) {
    return (
      <div className="app">
        <div className="loading-screen">
          <div className="loading-text">INITIALIZING</div>
          <div className="loading-cursor">&#x258D;</div>
        </div>
      </div>
    )
  }

  return (
    <div className="app" data-system={activeSystem}>
      <TitleBar status={titleStatus} system={activeSystem} isRunning={isRunning} />
      <TabBar
        active={activeSystem}
        onChange={handleTabChange}
        isTransitioning={isTransitioning}
      />
      <CrtScreen
        modRef={modRef}
        activeCtxRef={activeCtxRef}
        isRunningRef={isRunningRef}
        isTransitioning={isTransitioning}
        hasRom={hasRom}
        flashAnim={flashAnim}
        wakeAnim={wakeAnim}
        onFps={setFps}
      />
      <ControlBar
        activeSystem={activeSystem}
        hasRom={hasRom}
        isRunning={isRunning}
        romName={romName}
        onOpenRom={handleOpenRom}
        onReset={handleReset}
        onTogglePlay={handleTogglePlay}
        onRegionChange={handleRegionChange}
      />
      <StatusBar status={statusText} fps={fps} />
    </div>
  )
}

export default App
