import assert from 'node:assert/strict'
import { readFile } from 'node:fs/promises'
import test from 'node:test'

import { transformWithEsbuild } from 'vite'

const sourceUrl = new URL('../src/webAudio.ts', import.meta.url)
const source = await readFile(sourceUrl, 'utf8')
const transformed = await transformWithEsbuild(source, sourceUrl.pathname, {
  loader: 'ts',
  format: 'esm',
  target: 'es2020',
})
const { NesWebAudioPlayer } = await import(
  `data:text/javascript;base64,${Buffer.from(transformed.code).toString('base64')}`
)

class FakeAudioBuffer {
  channels
  duration

  constructor(channelCount, frameCount, sampleRate) {
    this.channels = Array.from({ length: channelCount }, () => new Float32Array(frameCount))
    this.duration = frameCount / sampleRate
  }

  getChannelData(channel) {
    return this.channels[channel]
  }
}

class FakeBufferSource {
  buffer = null
  onended = null
  startTime = null
  stopCount = 0

  connect() {}

  start(time) {
    this.startTime = time
  }

  stop() {
    this.stopCount++
  }
}

class FakeAudioContext {
  state = 'suspended'
  currentTime = 10
  destination = {}
  resumeCount = 0
  closeCount = 0
  sources = []

  async resume() {
    this.resumeCount++
    this.state = 'running'
  }

  async close() {
    this.closeCount++
    this.state = 'closed'
  }

  createBuffer(channelCount, frameCount, sampleRate) {
    return new FakeAudioBuffer(channelCount, frameCount, sampleRate)
  }

  createBufferSource() {
    const source = new FakeBufferSource()
    this.sources.push(source)
    return source
  }
}

test('unlock creates and resumes one audio context', () => {
  const context = new FakeAudioContext()
  let createCount = 0
  const player = new NesWebAudioPlayer(() => {
    createCount++
    return context
  })

  player.unlock()
  player.unlock()

  assert.equal(createCount, 1)
  assert.equal(context.resumeCount, 1)
  assert.equal(context.state, 'running')
})

test('stereo PCM frames are deinterleaved and scheduled continuously', () => {
  const context = new FakeAudioContext()
  const player = new NesWebAudioPlayer(() => context)
  player.unlock()

  assert.equal(player.enqueueStereoS16(Int16Array.of(16384, -16384, 8192, -8192), 2), true)
  assert.equal(player.enqueueStereoS16(Int16Array.of(4096, -4096), 1), true)

  const first = context.sources[0]
  const second = context.sources[1]
  assert.deepEqual([...first.buffer.channels[0]], [0.5, 0.25])
  assert.deepEqual([...first.buffer.channels[1]], [-0.5, -0.25])
  assert.equal(first.startTime, 10.03)
  assert.equal(second.startTime, first.startTime + first.buffer.duration)
})

test('reset stops queued audio and restarts with a short lead', () => {
  const context = new FakeAudioContext()
  const player = new NesWebAudioPlayer(() => context)
  player.unlock()
  player.enqueueStereoS16(Int16Array.of(1, 1), 1)

  player.reset()
  context.currentTime = 20
  player.enqueueStereoS16(Int16Array.of(2, 2), 1)

  assert.equal(context.sources[0].stopCount, 1)
  assert.equal(context.sources[1].startTime, 20.03)
})

test('packets are rejected while browser audio is suspended', () => {
  const context = new FakeAudioContext()
  context.resume = async () => {}
  const player = new NesWebAudioPlayer(() => context)
  player.unlock()

  assert.equal(player.enqueueStereoS16(Int16Array.of(1, 1), 1), false)
  assert.equal(context.sources.length, 0)
})
