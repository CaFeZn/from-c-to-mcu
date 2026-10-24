import { busMode, FLASH_EXAMPLE, FLASH_STEPS } from './mcu-architecture.mjs'

// These are semantic observation windows, never AHB clocks or sample edges.
export function parallelWord(value, active = false) {
  if (value !== null && (!Number.isInteger(value) || value < 0 || value > 0xffffffff)) {
    throw new RangeError('Expected an unsigned 32-bit word or null')
  }
  const known = active === true && value !== null
  return {
    status: active !== true ? 'unspecified' : known ? 'valid' : 'unmodeled',
    value: known ? value : null,
    bits: Array.from({ length: 32 }, (_, offset) => {
      const index = 31 - offset
      return { index, level: known ? (value >>> index) & 1 : null }
    }),
  }
}

export function architectureSignals(state) {
  const mode = busMode(state.mode)
  return {
    address: mode.address,
    addressActive: state.step === 1 || state.step === 2,
    value: typeof state.payload === 'number' ? state.payload : null,
    active: state.step === 3,
    direction: mode.direction,
    unmodeled: mode.id === 'flash' ? '指令位未建模' : '数据位未建模',
  }
}

export function flashFlowPayloads(state) {
  const step = FLASH_STEPS[state.step].id
  const result = { flash: null, ram: null, core: null }
  if (step === 'load-msp') result.flash = '0x' + FLASH_EXAMPLE.msp.toString(16).toUpperCase()
  if (step === 'load-reset-vector') result.flash = '0x' + FLASH_EXAMPLE.resetVector.toString(16).toUpperCase().padStart(8, '0')
  if (step === 'fetch-startup' || step === 'enter-main') result.flash = '指令位未建模'
  if (step === 'copy-data') result.flash = result.ram = String(state.flashInitial)
  if (step === 'zero-bss') result.ram = '0'
  if (step === 'load-counter') result.ram = String(state.ram.counter)
  if (step === 'increment') result.core = String(state.accumulator - 1) + ' + 1 = ' + state.accumulator
  if (step === 'store-counter') result.ram = String(state.accumulator)
  return result
}
