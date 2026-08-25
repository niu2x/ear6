import './StatusBar.css'

interface StatusBarProps {
  status: string
  fps: number
}

export function StatusBar({ status, fps }: StatusBarProps) {
  return (
    <div className="status-bar">
      <span className="status-text">{status}</span>
      <span className="status-fps">{fps} FPS</span>
    </div>
  )
}
