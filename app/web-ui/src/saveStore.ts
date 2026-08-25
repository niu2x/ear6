const STORAGE_PREFIX = 'ear6.save.v1.'
const STORAGE_VERSION = 1
const BASE64_CHUNK_SIZE = 0x8000

export interface StoredSave {
  version: 1
  systemType: number
  contentIdentity: string
  contentName: string
  savedAt: string
  previewDataUrl: string | null
  stateBase64: string
}

function isStoredSave(value: unknown): value is StoredSave {
  if (!value || typeof value !== 'object') return false
  const save = value as Record<string, unknown>
  return save.version === STORAGE_VERSION
    && Number.isInteger(save.systemType)
    && typeof save.contentIdentity === 'string'
    && /^[0-9a-f]{16}$/.test(save.contentIdentity)
    && typeof save.contentName === 'string'
    && typeof save.savedAt === 'string'
    && !Number.isNaN(Date.parse(save.savedAt))
    && (save.previewDataUrl === null
      || (typeof save.previewDataUrl === 'string'
        && save.previewDataUrl.startsWith('data:image/png;base64,')))
    && typeof save.stateBase64 === 'string'
}

export function saveStorageKey(systemType: number, contentIdentity: string) {
  return `${STORAGE_PREFIX}${systemType}.${contentIdentity}`
}

export function encodeState(bytes: Uint8Array) {
  let binary = ''
  for (let offset = 0; offset < bytes.length; offset += BASE64_CHUNK_SIZE) {
    binary += String.fromCharCode(...bytes.subarray(offset, offset + BASE64_CHUNK_SIZE))
  }
  return btoa(binary)
}

export function decodeState(value: string) {
  const binary = atob(value)
  const bytes = new Uint8Array(binary.length)
  for (let index = 0; index < binary.length; index += 1) {
    bytes[index] = binary.charCodeAt(index)
  }
  return bytes
}

export function listStoredSaves(storage: Storage = window.localStorage) {
  const saves: StoredSave[] = []
  for (let index = 0; index < storage.length; index += 1) {
    const key = storage.key(index)
    if (!key?.startsWith(STORAGE_PREFIX)) continue
    const raw = storage.getItem(key)
    if (!raw) continue
    try {
      const value: unknown = JSON.parse(raw)
      if (isStoredSave(value)
        && key === saveStorageKey(value.systemType, value.contentIdentity)) {
        saves.push(value)
      }
    } catch {
      // Ignore malformed or manually edited entries without hiding valid saves.
    }
  }
  return saves.sort((left, right) => Date.parse(right.savedAt) - Date.parse(left.savedAt))
}

export function writeStoredSave(save: StoredSave, storage: Storage = window.localStorage) {
  storage.setItem(saveStorageKey(save.systemType, save.contentIdentity), JSON.stringify(save))
}

export function removeStoredSave(save: StoredSave, storage: Storage = window.localStorage) {
  storage.removeItem(saveStorageKey(save.systemType, save.contentIdentity))
}
