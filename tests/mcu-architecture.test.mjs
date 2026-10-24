import test from 'node:test'
import assert from 'node:assert/strict'
import { readFile } from 'node:fs/promises'
import { createRequire } from 'node:module'
import { createServer } from 'node:http'
import { fileURLToPath } from 'node:url'
import { parseHTML } from 'linkedom'
import {
  BUS_MODES, BUS_PHASES, FLASH_EXAMPLE, FLASH_STEPS, MCU_TOPOLOGY,
  advanceBus, advanceFlash, busExplanation, busMode,
  createBusState, createFlashState, hexAddress, switchBusMode,
} from '../docs/.vitepress/theme/mcu-architecture.mjs'

const root = fileURLToPath(new URL('../', import.meta.url))
const require = createRequire(import.meta.url)
const components = ['McuArchitectureLab', 'FlashExecutionLab']
const finishBus = mode => Array.from({ length: 4 }).reduce(advanceBus, createBusState(mode))
const flashTrace = () => {
  const states = [createFlashState()]
  for (let i = 1; i < FLASH_STEPS.length; i++) states.push(advanceFlash(states.at(-1)))
  return states
}

test('bus reset is an explicit example state, with no pending access', () => {
  for (const mode of BUS_MODES) {
    const state = createBusState(mode.id)
    assert.equal(state.step, 0)
    assert.equal(state.mode, mode.id)
    assert.equal(state.pc, 0x08000400)
    assert.equal(state.r0, 0)
    assert.equal(state.pa5, false)
    assert.equal(state.instructionReady, false)
    assert.equal(state.address, null)
    assert.equal(state.target, null)
    assert.equal(state.payload, null)
    assert.equal(state.gpioWrite, null)
    assert.equal(state.sramValue, 42)
  }
  assert.throws(() => createBusState('arbitrary-code'), RangeError)
})

test('each bus mode separates address, selection, transfer and state update', () => {
  for (const mode of BUS_MODES) {
    let previous = createBusState(mode.id)
    for (let step = 1; step <= 4; step++) {
      const snapshot = structuredClone(previous)
      const next = advanceBus(previous)
      assert.deepEqual(previous, snapshot)
      assert.equal(next.step, step)
      assert.equal(next.address, mode.address)
      assert.equal(next.target, step >= 2 ? mode.target : null)
      assert.equal(next.payload, step >= 3 ? mode.payload : null)
      assert.equal(next.r0, mode.id === 'sram' && step === 4 ? 42 : 0)
      assert.equal(next.pa5, mode.id === 'gpio' && step === 4)
      assert.equal(next.instructionReady, mode.id === 'flash' && step === 4)
      assert.equal(next.pc, 0x08000400)
      assert.ok(busExplanation(next).length > 0)
      assert.ok(BUS_PHASES[next.step])
      previous = next
    }
    assert.equal(advanceBus(previous), previous)
  }
})

test('GPIO is an AHB1 write of BSRR bit 5, not a register read', () => {
  const mode = busMode('gpio')
  const state = finishBus('gpio')
  assert.equal(MCU_TOPOLOGY.gpioBus, 'AHB1')
  assert.equal(mode.address, 0x40020018)
  assert.equal(mode.direction, 'write')
  assert.equal(state.gpioWrite, 0x20)
  assert.equal(state.pa5, true)
  assert.equal(state.r0, 0)
  assert.match(MCU_TOPOLOGY.gpioAssumption, /时钟已开启/)
})

test('mode switches discard every prior mode result and never mutate it', () => {
  for (const from of BUS_MODES) for (const to of BUS_MODES) {
    const previous = finishBus(from.id)
    const snapshot = structuredClone(previous)
    assert.deepEqual(switchBusMode(previous, to.id), createBusState(to.id))
    assert.deepEqual(previous, snapshot)
  }
  assert.notEqual(createBusState(), createBusState())
})

test('Flash reset preserves programmed initial data while SRAM starts unknown', () => {
  const state = createFlashState()
  assert.equal(state.step, 0)
  assert.equal(state.mapped, false)
  for (const field of ['msp', 'resetVector', 'thumb', 'pc', 'accumulator']) assert.equal(state[field], null)
  assert.deepEqual(state.ram, { counter: null, events: null })
  assert.equal(state.flashInitial, 7)
  assert.equal(state.codeLocation, 'Flash')
  const independent = createFlashState()
  state.ram.counter = 123
  assert.equal(independent.ram.counter, null)
})

test('hardware reset loads MSP and separates the Thumb flag from the entry', () => {
  const states = flashTrace()
  assert.equal(states[1].mapped, true)
  assert.equal(states[1].msp, null)
  assert.equal(states[2].msp, 0x20020000)
  assert.equal(states[2].resetVector, null)
  assert.equal(states[3].resetVector, 0x08000201)
  assert.equal(states[3].thumb, true)
  assert.equal(states[3].pc, 0x08000200)
  assert.equal(states[3].pc, states[3].resetVector & ~1)
  assert.deepEqual(states[3].ram, { counter: null, events: null })
  assert.equal(FLASH_EXAMPLE.aliasBase, 0)
  assert.equal(FLASH_EXAMPLE.flashBase, 0x08000000)
})

test('software initializes data before bss, then reads, increments and writes RAM', () => {
  const states = flashTrace()
  for (const state of states.slice(0, 5)) assert.deepEqual(state.ram, { counter: null, events: null })
  assert.deepEqual(states[5].ram, { counter: 7, events: null })
  assert.deepEqual(states[6].ram, { counter: 7, events: 0 })
  assert.equal(states[7].pc, 0x08000400)
  assert.equal(states[7].instruction, 'main')
  assert.equal(states[8].accumulator, 7)
  assert.equal(states[9].accumulator, 8)
  assert.equal(states[9].ram.counter, 7)
  assert.deepEqual(states[10].ram, { counter: 8, events: 0 })
  assert.equal(FLASH_EXAMPLE.dataSource, 0x08000800)
  assert.equal(FLASH_EXAMPLE.counterAddress, 0x20000000)
  assert.equal(FLASH_EXAMPLE.eventsAddress, 0x20000004)
})

test('Flash values and code location never change and reset creates a clean trace', () => {
  let previous = createFlashState()
  for (let i = 1; i < FLASH_STEPS.length; i++) {
    const snapshot = structuredClone(previous)
    const next = advanceFlash(previous)
    assert.deepEqual(previous, snapshot)
    assert.notEqual(next.ram, previous.ram)
    assert.equal(next.flashInitial, 7)
    assert.equal(next.codeLocation, 'Flash')
    previous = next
  }
  assert.equal(advanceFlash(previous), previous)
  const reset = createFlashState()
  assert.deepEqual(reset.ram, { counter: null, events: null })
  assert.equal(reset.instruction, '未取指')
  assert.equal(reset.step, 0)
  assert.equal(hexAddress(0), '0x00000000')
  assert.equal(hexAddress(FLASH_EXAMPLE.resetEntry), '0x08000200')
  assert.ok(FLASH_EXAMPLE.resetEntry >= FLASH_EXAMPLE.flashBase + 0x200)
  assert.ok(FLASH_EXAMPLE.mainEntry > FLASH_EXAMPLE.resetEntry)
  assert.equal(FLASH_EXAMPLE.resetVector, FLASH_EXAMPLE.resetEntry | 1)
  assert.equal(hexAddress(null), '未发生')
})

// Compile directly to memory: these checks do not need a course route or generated files.
async function bundleComponents(ssr) {
  const { parse, compileScript, compileStyle } = await import('vue/compiler-sfc')
  const viteRequire = createRequire(require.resolve('vitepress/package.json'))
  const { build } = viteRequire('esbuild')
  const plugin = {
    name: 'lab-vue-in-memory',
    setup(build) {
      build.onLoad({ filter: /\.vue$/ }, async ({ path }) => {
        const source = await readFile(path, 'utf8')
        const { descriptor, errors } = parse(source, { filename: path })
        assert.deepEqual(errors, [])
        const id = path.includes('McuArchitectureLab') ? 'mcu-lab-test' : 'flash-lab-test'
        const script = compileScript(descriptor, {
          id, inlineTemplate: true, genDefaultAs: '__component',
          templateOptions: { ssr, compilerOptions: { scopeId: 'data-v-' + id } },
        })
        const styles = descriptor.styles.map(style => {
          const result = compileStyle({ id: 'data-v-' + id, source: style.content, scoped: style.scoped, filename: path })
          assert.deepEqual(result.errors, [])
          return result.code
        }).join('\n')
        const injection = ssr ? '' : 'const style = document.createElement("style"); style.textContent = ' + JSON.stringify(styles) + '; document.head.append(style);'
        return { contents: script.content + '\n__component.__scopeId = ' + JSON.stringify('data-v-' + id) + ';\n' + injection + '\nexport default __component;', loader: 'js', resolveDir: fileURLToPath(new URL('../docs/.vitepress/theme/components/', import.meta.url)) }
      })
    },
  }
  const imports = components.map(name => 'import ' + name + ' from "./docs/.vitepress/theme/components/' + name + '.vue";').join('\n')
  const browserEntry = 'import { createApp, h, ref } from "vue"; const bus = ref(true), flash = ref(true); const app = createApp({ setup: () => () => h("main", { class: "vp-doc" }, [bus.value ? h(McuArchitectureLab) : null, flash.value ? h(FlashExecutionLab) : null]) }); app.mount("#app"); window.__labs = { hideBus: () => { bus.value = false }, hideFlash: () => { flash.value = false } };'
  const result = await build({
    stdin: { contents: imports + '\n' + (ssr ? 'export { McuArchitectureLab, FlashExecutionLab };' : browserEntry), resolveDir: root, sourcefile: 'mcu-lab-check.mjs' },
    absWorkingDir: root, plugins: [plugin], bundle: true, write: false,
    platform: ssr ? 'node' : 'browser', format: ssr ? 'cjs' : 'esm',
    packages: ssr ? 'external' : undefined, logLevel: 'silent',
    define: { __VUE_OPTIONS_API__: 'true', __VUE_PROD_DEVTOOLS__: 'false', __VUE_PROD_HYDRATION_MISMATCH_DETAILS__: 'false' },
  })
  return result.outputFiles[0].text
}

test('both components compile and SSR-render initial state without browser APIs or timers', async () => {
  const { createSSRApp } = await import('vue')
  const { renderToString } = await import('vue/server-renderer')
  const source = await bundleComponents(true)
  const module = { exports: {} }
  new Function('require', 'module', 'exports', source)(require, module, module.exports)
  const original = globalThis.setInterval
  globalThis.setInterval = () => { throw new Error('SSR must not start playback') }
  try {
    const bus = await renderToString(createSSRApp(module.exports.McuArchitectureLab))
    const flash = await renderToString(createSSRApp(module.exports.FlashExecutionLab))
    const { document } = parseHTML('<html><body>' + bus + flash + '</body></html>')
    const byId = id => document.querySelector('[data-testid="' + id + '"]')
    for (const id of ['mcu-bus-lab', 'flash-execution-lab']) {
      assert.equal(byId(id).getAttribute('data-step'), '0')
      assert.equal(byId(id).getAttribute('data-playing'), 'false')
    }
    assert.equal(byId('flash-ram-counter').textContent, '未知')
    assert.equal(byId('flash-ram-events').textContent, '未知')
    assert.equal(byId('flash-initial-value').textContent, '7')
  } finally {
    globalThis.setInterval = original
  }
})

test('browser: controls, isolation, timer cleanup and 320/390/1280px layout', { skip: process.env.MCU_LAB_BROWSER !== '1', timeout: 120000 }, async () => {
  const { chromium } = await import('@playwright/test')
  const script = await bundleComponents(false)
  const theme = await readFile(new URL('../docs/.vitepress/theme/style.css', import.meta.url), 'utf8')
  const html = '<!doctype html><html><head><meta name="viewport" content="width=device-width, initial-scale=1"><style>' + theme + 'html,body{margin:0} .vp-doc{box-sizing:border-box;width:100%;max-width:860px;margin:auto;padding:20px} </style></head><body><div id="app"></div><script type="module" src="/lab.js"></script></body></html>'
  const server = createServer((request, response) => {
    response.setHeader('Content-Type', request.url === '/lab.js' ? 'text/javascript; charset=utf-8' : 'text/html; charset=utf-8')
    response.end(request.url === '/lab.js' ? script : html)
  })
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve))
  let browser
  try {
    browser = await chromium.launch({ channel: process.env.CI ? undefined : 'chrome' })
    for (const width of [320, 390, 1280]) {
      const context = await browser.newContext({ viewport: { width, height: 900 }, reducedMotion: 'reduce' })
      const page = await context.newPage()
      const errors = []
      page.on('pageerror', error => errors.push(error.message))
      await page.clock.install()
      await page.addInitScript(() => {
        const intervals = new Set()
        const start = window.setInterval.bind(window), stop = window.clearInterval.bind(window)
        window.setInterval = (...args) => { const id = start(...args); intervals.add(id); return id }
        window.clearInterval = id => { intervals.delete(id); stop(id) }
        window.__intervals = intervals
      })
      await page.goto('http://127.0.0.1:' + server.address().port)
      const byId = id => page.getByTestId(id)
      const text = id => byId(id).innerText()
      const step = id => byId(id).getAttribute('data-step')
      const click = id => byId(id).click()
      const noOverflow = async () => {
        const overflow = await page.evaluate(() => ({
          document: document.documentElement.scrollWidth > window.innerWidth,
          labs: [...document.querySelectorAll('[data-testid$="-lab"]')].filter(el => el.scrollWidth > el.clientWidth + 1).map(el => el.dataset.testid),
          elements: [...document.querySelectorAll('.architecture-lab *, .flash-execution *')].filter(el => { const rect = el.getBoundingClientRect(); return rect.width > 0 && (rect.right > innerWidth + 1 || rect.left < -1) }).map(el => el.tagName + '.' + el.className),
        }))
        assert.deepEqual(overflow, { document: false, labs: [], elements: [] }, 'overflow at ' + width)
      }
      await byId('mcu-bus-lab').waitFor()
      if (width === 390) await page.evaluate(() => document.documentElement.classList.add('dark'))
      await page.clock.runFor(3000)
      assert.equal(await step('mcu-bus-lab'), '0')
      assert.equal(await step('flash-execution-lab'), '0')
      assert.equal(await page.evaluate(() => window.__intervals.size), 0)
      await noOverflow()
      for (const mode of BUS_MODES) {
        await click('mcu-mode-' + mode.id)
        for (let i = 1; i <= 4; i++) {
          await click('mcu-bus-next')
          assert.equal(await step('mcu-bus-lab'), String(i))
          assert.equal(await text('mcu-bus-address'), hexAddress(mode.address))
          assert.equal(await text('mcu-bus-r0'), mode.id === 'sram' && i === 4 ? '42' : '0')
          assert.equal(await text('mcu-bus-pa5'), mode.id === 'gpio' && i === 4 ? '高电平（1）' : '低电平（0）')
          await noOverflow()
        }
        assert.equal(await byId('mcu-bus-next').isDisabled(), true)
        await click('mcu-bus-reset')
        assert.equal(await text('mcu-bus-address'), '未发生')
      }
      await click('mcu-bus-play')
      await page.clock.runFor(1100)
      await click('mcu-bus-play')
      await page.clock.runFor(2500)
      assert.equal(await step('mcu-bus-lab'), '1')
      await click('mcu-bus-play')
      await click('mcu-mode-sram')
      await page.clock.runFor(5000)
      assert.equal(await step('mcu-bus-lab'), '0')
      assert.equal(await step('flash-execution-lab'), '0')
      const states = flashTrace()
      for (const state of states.slice(1)) {
        await click('flash-next')
        assert.equal(await step('flash-execution-lab'), String(state.step))
        assert.equal(await text('flash-ram-counter'), state.ram.counter === null ? '未知' : String(state.ram.counter))
        assert.equal(await text('flash-ram-events'), state.ram.events === null ? '未知' : String(state.ram.events))
        assert.equal(await text('flash-initial-value'), '7')
        await noOverflow()
      }
      assert.equal(await text('flash-reset-vector'), '0x08000201')
      assert.equal(await text('flash-reset-entry'), '0x08000200')
      assert.equal(await text('flash-pc'), '0x08000400')
      assert.equal(await byId('flash-next').isDisabled(), true)
      await click('flash-reset')
      assert.equal(await text('flash-ram-counter'), '未知')
      assert.equal(await text('flash-ram-events'), '未知')
      await click('flash-play')
      await page.clock.runFor(1400)
      await click('flash-play')
      await page.clock.runFor(3000)
      assert.equal(await step('flash-execution-lab'), '1')
      await click('flash-reset')
      await click('mcu-bus-play')
      await click('flash-play')
      assert.equal(await page.evaluate(() => window.__intervals.size), 2)
      await page.clock.runFor(3300)
      assert.equal(await step('mcu-bus-lab'), '3')
      assert.equal(await byId('mcu-bus-lab').locator('.arch-transfer.visible').evaluate(el => getComputedStyle(el).animationName), 'none')
      await click('mcu-bus-reset')
      assert.equal(await page.evaluate(() => window.__intervals.size), 1)
      await page.evaluate(() => window.__labs.hideFlash())
      await byId('flash-execution-lab').waitFor({ state: 'detached' })
      assert.equal(await page.evaluate(() => window.__intervals.size), 0)
      await click('mcu-bus-play')
      await page.evaluate(() => window.__labs.hideBus())
      await byId('mcu-bus-lab').waitFor({ state: 'detached' })
      assert.equal(await page.evaluate(() => window.__intervals.size), 0)
      assert.deepEqual(errors, [])
      await context.close()
    }
  } finally {
    await browser?.close()
    await new Promise(resolve => server.close(resolve))
  }
})
