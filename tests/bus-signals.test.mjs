import test from 'node:test'
import assert from 'node:assert/strict'
import { readFile } from 'node:fs/promises'
import { createRequire } from 'node:module'
import { dirname, resolve } from 'node:path'
import { fileURLToPath, pathToFileURL } from 'node:url'
import { createSSRApp } from 'vue'
import { renderToString } from 'vue/server-renderer'
import { compileScript, compileStyle, parse } from 'vue/compiler-sfc'
import { parseHTML } from 'linkedom'
import { architectureSignals, flashFlowPayloads, parallelWord } from '../docs/.vitepress/theme/bus-signals.mjs'
import { BUS_MODES, FLASH_STEPS, advanceBus, advanceFlash, createBusState, createFlashState } from '../docs/.vitepress/theme/mcu-architecture.mjs'

const require = createRequire(import.meta.url)

test('all 32 parallel levels represent one word including unsigned bit 31', () => {
  for (const value of [0, 0x20, 42, 0x80000000, 0x80008000, 0x00200020, 0xffffffff]) {
    const word = parallelWord(value, true)
    assert.equal(word.status, 'valid')
    assert.equal(word.value, value)
    assert.deepEqual(word.bits.map(bit => bit.index), Array.from({ length: 32 }, (_, i) => 31 - i))
    assert.equal(word.bits.reduce((sum, bit) => sum + bit.level * 2 ** bit.index, 0), value)
  }
  assert.deepEqual(parallelWord(42, true).bits.filter(bit => bit.level === 1).map(bit => bit.index), [5, 3, 1])
  assert.deepEqual(parallelWord(0x20, true).bits.filter(bit => bit.level === 1).map(bit => bit.index), [5])
})

test('outside a window and unmodeled instructions never become zero or high impedance', () => {
  for (const value of [0, 42, null]) {
    const word = parallelWord(value, false)
    assert.equal(word.status, 'unspecified')
    assert.equal(word.value, null)
    assert.ok(word.bits.every(bit => bit.level === null))
  }
  assert.equal(parallelWord(null, true).status, 'unmodeled')
  assert.ok(parallelWord(null, true).bits.every(bit => bit.level === null))
  assert.equal(parallelWord(0, 1).status, 'unspecified')
  for (const value of [-1, 0x100000000, 1.5, NaN, Infinity, '42', undefined]) assert.throws(() => parallelWord(value, true), RangeError)
})

test('architecture address and data observation windows follow semantic stages, not clock cycles', () => {
  for (const mode of BUS_MODES) {
    let state = createBusState(mode.id)
    for (let step = 0; step <= 4; step++) {
      const signals = architectureSignals(state)
      assert.equal(signals.address, mode.address)
      assert.equal(signals.addressActive, step === 1 || step === 2)
      assert.equal(signals.active, step === 3)
      assert.equal(signals.direction, mode.direction)
      const word = parallelWord(signals.value, signals.active)
      assert.equal(word.status, step !== 3 ? 'unspecified' : mode.id === 'flash' ? 'unmodeled' : 'valid')
      if (step === 3 && mode.id !== 'flash') assert.equal(word.value, mode.payload)
      if (mode.id === 'flash') assert.equal(signals.value, null)
      state = advanceBus(state)
    }
    assert.ok(parallelWord(architectureSignals(createBusState(mode.id)).value).bits.every(bit => bit.level === null))
  }
})

test('Flash payload markers use model data and never invent machine code', () => {
  let state = createFlashState()
  const payloads = []
  for (let step = 0; step < FLASH_STEPS.length; step++) {
    const before = structuredClone(state)
    payloads.push(flashFlowPayloads(state))
    assert.deepEqual(state, before)
    state = advanceFlash(state)
  }
  assert.deepEqual(payloads[0], { flash: null, ram: null, core: null })
  assert.equal(payloads[2].flash, '0x20020000')
  assert.equal(payloads[3].flash, '0x08000201')
  assert.equal(payloads[4].flash, '指令位未建模')
  assert.deepEqual(payloads[5], { flash: '7', ram: '7', core: null })
  assert.deepEqual(payloads[6], { flash: null, ram: '0', core: null })
  assert.equal(payloads[7].flash, '指令位未建模')
  assert.equal(payloads[8].ram, '7')
  assert.deepEqual(payloads[9], { flash: null, ram: null, core: '7 + 1 = 8' })
  assert.equal(payloads[10].ram, '8')
  assert.equal(flashFlowPayloads({ ...createFlashState(), step: 5, flashInitial: 9 }).ram, '9')
})

async function signalComponent() {
  const filename = fileURLToPath(new URL('../docs/.vitepress/theme/components/BusSignals.vue', import.meta.url))
  const source = await readFile(filename, 'utf8')
  const { descriptor, errors } = parse(source, { filename })
  assert.deepEqual(errors, [])
  for (const style of descriptor.styles) {
    const result = compileStyle({ filename, id: 'data-v-signals-test', source: style.content, scoped: style.scoped })
    assert.deepEqual(result.errors, [])
    assert.ok(result.code.length > 0)
  }
  // Compile both actual template targets. A missing dependency or syntax error must fail.
  compileScript(descriptor, { id: 'signals-test', inlineTemplate: true })
  const compiled = compileScript(descriptor, { id: 'signals-test', inlineTemplate: true, templateOptions: { ssr: true } })
  const code = compiled.content.replace(/from (['"])([^'"]+)\1/g, (_, _quote, specifier) => {
    const absolute = specifier.startsWith('.') ? resolve(dirname(filename), specifier) : require.resolve(specifier)
    return 'from ' + JSON.stringify(pathToFileURL(absolute).href)
  })
  return (await import('data:text/javascript;base64,' + Buffer.from(code).toString('base64'))).default
}

test('signal SFC compiles and SSR renders known, unknown and blocked states without timers', async () => {
  const component = await signalComponent()
  const interval = globalThis.setInterval
  globalThis.setInterval = () => { throw new Error('Signal SSR must not create timers') }
  try {
    for (const [props, status, highs] of [
      [{}, 'unspecified', []],
      [{ value: 0x20, active: true }, 'valid', [5]],
      [{ value: 42, active: true }, 'valid', [5, 3, 1]],
      [{ value: 0x80008000, active: true }, 'valid', [31, 15]],
      [{ value: 0, active: true }, 'valid', []],
      [{ active: true, unmodeled: '指令位未建模' }, 'unmodeled', []],
      [{ value: 0x20, active: true, blocked: true, playing: true }, 'unspecified', []],
      [{ value: 42, active: false, playing: true }, 'unspecified', []],
    ]) {
      const app = createSSRApp(component, props)
      app.config.warnHandler = message => { throw new Error(message) }
      const html = await renderToString(app)
      const { document } = parseHTML('<html><body>' + html + '</body></html>')
      const root = document.querySelector('[data-testid="bus-signals"]')
      const bits = [...root.querySelectorAll('[data-bit]')]
      assert.equal(root.dataset.status, status)
      assert.equal(bits.length, 32)
      assert.deepEqual(bits.filter(bit => bit.dataset.level === '1').map(bit => Number(bit.dataset.bit)), highs)
      if (status !== 'valid') {
        assert.ok(bits.every(bit => bit.dataset.level === 'unknown'))
        assert.equal(root.querySelector('.level-line'), null)
      }
      assert.doesNotMatch(html, /高阻|>Z</)
      if (props.blocked) assert.equal(root.classList.contains('running'), false)
    }
  } finally { globalThis.setInterval = interval }
})
