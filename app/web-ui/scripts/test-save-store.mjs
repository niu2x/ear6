import assert from 'node:assert/strict'
import { readFile } from 'node:fs/promises'
import test from 'node:test'

import { transformWithEsbuild } from 'vite'

const sourceUrl = new URL('../src/saveStore.ts', import.meta.url)
const source = await readFile(sourceUrl, 'utf8')
const transformed = await transformWithEsbuild(source, sourceUrl.pathname, {
  loader: 'ts',
  format: 'esm',
  target: 'es2020',
})
const store = await import(
  `data:text/javascript;base64,${Buffer.from(transformed.code).toString('base64')}`
)

class StorageMock {
  data = new Map()

  get length() {
    return this.data.size
  }

  key(index) {
    return [...this.data.keys()][index] ?? null
  }

  getItem(key) {
    return this.data.get(key) ?? null
  }

  setItem(key, value) {
    this.data.set(key, String(value))
  }

  removeItem(key) {
    this.data.delete(key)
  }
}

function makeSave(overrides = {}) {
  return {
    version: 1,
    systemType: 1,
    contentIdentity: '0000000000000001',
    contentName: 'A.nes',
    savedAt: '2026-08-25T01:00:00.000Z',
    previewDataUrl: null,
    stateBase64: 'c3RhdGU=',
    ...overrides,
  }
}

test('state bytes survive chunked base64 conversion', () => {
  const bytes = Uint8Array.from({ length: 100_000 }, (_, index) => index % 251)
  assert.deepEqual(store.decodeState(store.encodeState(bytes)), bytes)
})

test('a newer save replaces the same system and content identity', () => {
  const storage = new StorageMock()
  const first = makeSave()
  const newer = makeSave({
    savedAt: '2026-08-25T02:00:00.000Z',
    stateBase64: 'bmV3ZXI=',
  })

  store.writeStoredSave(first, storage)
  store.writeStoredSave(newer, storage)

  assert.equal(storage.length, 1)
  assert.deepEqual(store.listStoredSaves(storage), [newer])
})

test('listing sorts saves and ignores malformed records', () => {
  const storage = new StorageMock()
  const earlier = makeSave()
  const later = makeSave({
    contentIdentity: '0000000000000002',
    contentName: 'B.nes',
    savedAt: '2026-08-25T03:00:00.000Z',
  })
  store.writeStoredSave(earlier, storage)
  store.writeStoredSave(later, storage)
  storage.setItem('ear6.save.v1.bad', 'not-json')

  assert.deepEqual(store.listStoredSaves(storage), [later, earlier])
  store.removeStoredSave(later, storage)
  assert.deepEqual(store.listStoredSaves(storage), [earlier])
})
