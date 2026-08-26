const NES_AUDIO_SAMPLE_RATE = 96_000
const START_LEAD_SECONDS = 0.03
const MAX_QUEUED_SECONDS = 0.2

type AudioContextFactory = () => AudioContext

export class NesWebAudioPlayer {
  private context: AudioContext | null = null
  private nextStartTime = 0
  private readonly sources = new Set<AudioBufferSourceNode>()

  constructor(
    private readonly createContext: AudioContextFactory = () => new AudioContext(),
  ) {}

  unlock() {
    if (!this.context) {
      try {
        this.context = this.createContext()
      } catch {
        return
      }
    }

    if (this.context.state === 'suspended') {
      void this.context.resume().catch(() => {})
    }
  }

  enqueueStereoS16(samples: Int16Array, frameCount: number) {
    const context = this.context
    if (context?.state !== 'running' || frameCount <= 0 || samples.length < frameCount * 2) {
      return false
    }

    const now = context.currentTime
    if (this.nextStartTime > now + MAX_QUEUED_SECONDS) {
      this.reset()
    }

    const buffer = context.createBuffer(2, frameCount, NES_AUDIO_SAMPLE_RATE)
    const left = buffer.getChannelData(0)
    const right = buffer.getChannelData(1)
    for (let frame = 0; frame < frameCount; frame++) {
      left[frame] = samples[frame * 2] / 32768
      right[frame] = samples[frame * 2 + 1] / 32768
    }

    const source = context.createBufferSource()
    source.buffer = buffer
    source.connect(context.destination)
    source.onended = () => this.sources.delete(source)
    this.sources.add(source)

    const startTime = this.nextStartTime > now
      ? this.nextStartTime
      : now + START_LEAD_SECONDS
    source.start(startTime)
    this.nextStartTime = startTime + buffer.duration
    return true
  }

  reset() {
    for (const source of this.sources) {
      source.onended = null
      try {
        source.stop()
      } catch {
        // The source may already have ended between the state check and stop().
      }
    }
    this.sources.clear()
    this.nextStartTime = 0
  }

  close() {
    this.reset()
    const context = this.context
    this.context = null
    if (context && context.state !== 'closed') {
      void context.close().catch(() => {})
    }
  }
}
