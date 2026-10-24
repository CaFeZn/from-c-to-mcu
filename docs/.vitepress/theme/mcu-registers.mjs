export const UINT32_MAX = 0xffffffff
export const GPIOA_BASE = 0x40020000
export const GPIOA_ODR = GPIOA_BASE + 0x14
export const GPIOA_BSRR = GPIOA_BASE + 0x18
export const RCC_AHB1ENR = 0x40023830

export const ADDRESS_EXAMPLES = Object.freeze([
  { name: 'Flash', address: 0x08000400 },
  { name: 'SRAM', address: 0x20000000 },
  { name: 'GPIOA_MODER', address: GPIOA_BASE },
  { name: 'GPIOA_ODR', address: GPIOA_ODR },
  { name: 'GPIOA_BSRR', address: GPIOA_BSRR },
  { name: 'RCC_AHB1ENR', address: RCC_AHB1ENR },
].map(Object.freeze))

// Selected STM32F407xG regions, not a complete accessibility or permissions map.
export const MEMORY_REGIONS = Object.freeze([
  { id: 'flash', name: 'Flash', start: 0x08000000, end: 0x080fffff },
  { id: 'sram', name: 'SRAM1 / SRAM2', start: 0x20000000, end: 0x2001ffff },
  { id: 'peripheral', name: '外设映射', start: 0x40000000, end: 0x5fffffff },
].map(Object.freeze))

export const REGISTER_TARGETS = Object.freeze([
  { name: 'GPIOA_MODER', address: GPIOA_BASE, peripheral: 'GPIOA', base: GPIOA_BASE, offset: 0x00 },
  { name: 'GPIOA_ODR', address: GPIOA_ODR, peripheral: 'GPIOA', base: GPIOA_BASE, offset: 0x14 },
  { name: 'GPIOA_BSRR', address: GPIOA_BSRR, peripheral: 'GPIOA', base: GPIOA_BASE, offset: 0x18 },
  { name: 'RCC_AHB1ENR', address: RCC_AHB1ENR, peripheral: 'RCC', base: 0x40023800, offset: 0x30 },
].map(Object.freeze))

const PERIPHERALS = [
  { name: 'GPIOA', base: GPIOA_BASE, end: GPIOA_BASE + 0x3ff },
  { name: 'RCC', base: 0x40023800, end: 0x40023bff },
]

function assertUnsigned(value, maximum, name) {
  if (!Number.isSafeInteger(value) || value < 0 || value > maximum) {
    throw new RangeError(name + ' must be an unsigned integer in range')
  }
}

export function parseUint32Hex(input) {
  if (typeof input !== 'string') return { ok: false, code: 'invalid-hex', message: '请输入十六进制文本。' }
  const text = input.trim()
  if (!text) return { ok: false, code: 'empty', message: '地址或写入值不能为空。' }
  if (!/^(?:0[xX])?[0-9a-fA-F]+$/.test(text)) {
    return { ok: false, code: 'invalid-hex', message: '仅接受十六进制数字和可选 0x 前缀；不接受负数、符号或其他字符。' }
  }
  const digits = text.replace(/^0x/i, '').replace(/^0+/, '') || '0'
  if (digits.length > 8) return { ok: false, code: 'out-of-range', message: '超出 32 位范围：最大值为 0xFFFFFFFF。' }
  const value = Number.parseInt(digits, 16)
  return { ok: true, value, hex: formatHex32(value) }
}

export function formatHex32(value) {
  assertUnsigned(value, UINT32_MAX, 'value')
  return '0x' + value.toString(16).toUpperCase().padStart(8, '0')
}

export function formatHex16(value) {
  assertUnsigned(value, 0xffff, 'value')
  return '0x' + value.toString(16).toUpperCase().padStart(4, '0')
}

export function decodeAddress(address) {
  assertUnsigned(address, UINT32_MAX, 'address')
  const region = MEMORY_REGIONS.find(item => address >= item.start && address <= item.end) ?? null
  const common = { address, region, peripheral: null, base: null, offset: null, target: null, modeled: false }
  if (!region) return { ...common, status: 'unknown-region' }
  if (region.id !== 'peripheral') {
    return { ...common, base: region.start, offset: address - region.start, target: region.name, modeled: true, status: 'memory' }
  }
  const peripheral = PERIPHERALS.find(item => address >= item.base && address <= item.end)
  const routed = peripheral
    ? { ...common, peripheral: peripheral.name, base: peripheral.base, offset: address - peripheral.base }
    : common
  // The demo only models word-aligned 32-bit register operations.
  if (address % 4 !== 0) return { ...routed, status: 'unaligned' }
  const register = REGISTER_TARGETS.find(item => address === item.address)
  if (!register) return { ...routed, status: 'unknown-register' }
  return { ...routed, target: register.name, modeled: true, status: 'register' }
}

export function splitBsrr(writeValue) {
  assertUnsigned(writeValue, UINT32_MAX, 'writeValue')
  const setMask = writeValue & 0xffff
  const resetMask = writeValue >>> 16
  return {
    setMask,
    resetMask,
    conflictMask: setMask & resetMask,
    effectiveResetMask: resetMask & ~setMask & 0xffff,
  }
}

export function applyBsrr(odrBefore, writeValue) {
  assertUnsigned(odrBefore, 0xffff, 'odrBefore')
  const { setMask, resetMask } = splitBsrr(writeValue)
  // RM0090 GPIOx_BSRR: BSx has priority when BSx and BRx are both set.
  return ((odrBefore & ~resetMask) | setMask) & 0xffff
}

export function bsrrForPin(pin, command) {
  assertUnsigned(pin, 15, 'pin')
  if (command !== 'set' && command !== 'reset') throw new RangeError('command must be set or reset')
  return 2 ** (pin + (command === 'reset' ? 16 : 0))
}

export function bits16(value) {
  assertUnsigned(value, 0xffff, 'value')
  return Array.from({ length: 16 }, (_, position) => {
    const index = 15 - position
    return { index, on: (value & (2 ** index)) !== 0 }
  })
}

export function evaluateRegisterWrite({ odrBefore, writeValue, pin, clockEnabled, outputMode }) {
  assertUnsigned(odrBefore, 0xffff, 'odrBefore')
  assertUnsigned(pin, 15, 'pin')
  const masks = splitBsrr(writeValue)
  const missing = []
  if (clockEnabled !== true) missing.push('clock')
  if (outputMode !== true) missing.push('output-mode')
  const allowed = missing.length === 0
  const after = allowed ? applyBsrr(odrBefore, writeValue) : odrBefore
  return {
    allowed,
    missing,
    before: odrBefore,
    after,
    pinBefore: (odrBefore >>> pin) & 1,
    pinAfter: allowed ? (after >>> pin) & 1 : null,
    ...masks,
  }
}
