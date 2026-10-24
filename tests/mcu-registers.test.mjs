import test from 'node:test'
import assert from 'node:assert/strict'
import { readFile } from 'node:fs/promises'
import { createRequire, stripTypeScriptTypes } from 'node:module'
import { fileURLToPath, pathToFileURL } from 'node:url'
import { basename, dirname, resolve } from 'node:path'
import { createSSRApp } from 'vue'
import { renderToString } from 'vue/server-renderer'
import { compileScript, compileStyle, parse } from 'vue/compiler-sfc'
import {
  ADDRESS_EXAMPLES, GPIOA_BASE, GPIOA_BSRR, GPIOA_ODR, MEMORY_REGIONS, REGISTER_TARGETS,
  UINT32_MAX, applyBsrr, bits16, bsrrForPin, decodeAddress, evaluateRegisterWrite,
  formatHex16, formatHex32, parseUint32Hex, splitBsrr,
} from '../docs/.vitepress/theme/mcu-registers.mjs'

const require = createRequire(import.meta.url)
const componentDirectory = new URL('../docs/.vitepress/theme/components/', import.meta.url)

for (const [text, value] of [
  ['0', 0], ['0x00000000', 0], ['FFFFFFFF', UINT32_MAX], ['0Xffffffff', UINT32_MAX],
  ['  0x40020018  ', GPIOA_BSRR], ['00000000001', 1], ['80000000', 0x80000000], ['0b101', 0x0b101],
]) {
  test('hex parser accepts ' + JSON.stringify(text), () => {
    assert.deepEqual(parseUint32Hex(text), { ok: true, value, hex: formatHex32(value) })
  })
}

for (const text of ['', ' ', '-1', '-0x1', '+1', '0x', '0xGG', '0x20z', '12 34', '1.5', '1_000', '0xFFFFFFFFu', null, 32]) {
  test('hex parser rejects malformed input ' + JSON.stringify(text), () => assert.equal(parseUint32Hex(text).ok, false))
}

for (const text of ['0x100000000', '100000000', 'FFFFFFFFFFFFFFFF', '0x000100000000']) {
  test('hex parser rejects overflow ' + text, () => assert.equal(parseUint32Hex(text).code, 'out-of-range'))
}

test('hex formatting never silently wraps negative or oversized values', () => {
  assert.equal(formatHex32(UINT32_MAX), '0xFFFFFFFF')
  assert.equal(formatHex16(0xffff), '0xFFFF')
  for (const value of [-1, UINT32_MAX + 1, 1.5, NaN, Infinity, '32']) assert.throws(() => formatHex32(value), RangeError)
  assert.throws(() => formatHex16(0x10000), RangeError)
})

test('all offered examples resolve without claiming a read value', () => {
  for (const example of ADDRESS_EXAMPLES) {
    const result = decodeAddress(example.address)
    assert.equal(result.modeled, true)
    assert.equal(result.address, example.address)
    assert.equal('readValue' in result, false)
  }
})

test('register decode uses peripheral base plus exact register offset', () => {
  for (const register of REGISTER_TARGETS) {
    const result = decodeAddress(register.address)
    assert.equal(result.status, 'register')
    assert.equal(result.target, register.name)
    assert.equal(result.base, register.base)
    assert.equal(result.offset, register.offset)
    assert.equal(result.base + result.offset, result.address)
  }
  assert.equal(GPIOA_ODR, 0x40020014)
  assert.equal(GPIOA_BSRR, 0x40020018)
})

test('selected Flash and SRAM regions have bounded offsets', () => {
  assert.equal(decodeAddress(0x08000100).offset, 0x100)
  assert.equal(decodeAddress(0x20000000).offset, 0)
  for (const region of MEMORY_REGIONS.filter(item => item.id !== 'peripheral')) {
    assert.equal(decodeAddress(region.start).status, 'memory')
    assert.equal(decodeAddress(region.end).offset, region.end - region.start)
    assert.equal(decodeAddress(region.end + 1).modeled, false)
  }
})

test('unknown and reserved addresses do not gain invented targets or read values', () => {
  for (const address of [0, 0x10000000, 0x20020000, 0x60000000, UINT32_MAX]) {
    const result = decodeAddress(address)
    assert.equal(result.status, 'unknown-region')
    assert.equal(result.modeled, false)
    assert.equal(result.target, null)
  }
  for (const address of [GPIOA_BASE + 0x28, 0x40023834, 0x40020400]) {
    const result = decodeAddress(address)
    assert.equal(result.status, 'unknown-register')
    assert.equal(result.modeled, false)
    assert.equal(result.target, null)
  }
  assert.equal(decodeAddress(GPIOA_BASE + 0x28).offset, 0x28)
  assert.equal(decodeAddress(0x40020400).base, null)
})

test('word register alignment is explicit and never rounds to another register', () => {
  for (const offset of [1, 2, 3]) {
    const result = decodeAddress(GPIOA_BSRR + offset)
    assert.equal(result.status, 'unaligned')
    assert.equal(result.modeled, false)
    assert.equal(result.target, null)
    assert.equal(result.offset, 0x18 + offset)
  }
  assert.equal(decodeAddress(0x08000101).status, 'memory')
  for (const value of [-1, UINT32_MAX + 1, 1.2, NaN]) assert.throws(() => decodeAddress(value), RangeError)
})

test('BSRR splits command halves without signed bit-31 corruption', () => {
  assert.deepEqual(splitBsrr(0x80008000), { setMask: 0x8000, resetMask: 0x8000, conflictMask: 0x8000, effectiveResetMask: 0 })
  assert.deepEqual(splitBsrr(0x00200020), { setMask: 0x20, resetMask: 0x20, conflictMask: 0x20, effectiveResetMask: 0 })
})

test('BSRR sets low-half ones and resets high-half ones', () => {
  assert.equal(applyBsrr(0, 0x20), 0x20)
  assert.equal(applyBsrr(0xffff, 0x00200000), 0xffdf)
  assert.equal(applyBsrr(0x20, 0x00200000), 0)
})

test('BSRR both commands for the same pin use set priority', () => {
  assert.equal(applyBsrr(0, 0x00200020), 0x20)
  assert.equal(applyBsrr(0x20, 0x00200020), 0x20)
  assert.equal(applyBsrr(0, UINT32_MAX), 0xffff)
})

test('zero command changes no ODR bits', () => {
  for (const odr of [0, 0x20, 0x8000, 0xa55a, 0xffff]) assert.equal(applyBsrr(odr, 0), odr)
})

test('bit 15 set/reset shortcuts remain positive 32-bit integers', () => {
  assert.equal(bsrrForPin(15, 'set'), 0x00008000)
  assert.equal(bsrrForPin(15, 'reset'), 0x80000000)
  assert.equal(applyBsrr(0x8000, bsrrForPin(15, 'reset')), 0)
  assert.equal(applyBsrr(0, bsrrForPin(15, 'set')), 0x8000)
})

test('every pin command preserves all other latch bits', () => {
  for (let pin = 0; pin < 16; pin += 1) {
    const bit = 2 ** pin
    for (const before of [0, 0x5aa5, 0xa55a, 0xffff]) {
      for (const command of ['set', 'reset']) {
        const after = applyBsrr(before, bsrrForPin(pin, command))
        assert.equal(after & ~bit, before & ~bit)
        assert.equal((after >>> pin) & 1, command === 'set' ? 1 : 0)
      }
    }
  }
})

test('bit view is ordered 15 to 0 with a stable sixteen cells', () => {
  const bits = bits16(0x8001)
  assert.equal(bits.length, 16)
  assert.deepEqual(bits[0], { index: 15, on: true })
  assert.deepEqual(bits[15], { index: 0, on: true })
  assert.equal(bits.filter(bit => bit.on).length, 2)
})

test('model prerequisites fail closed, preserve ODR, and do not claim pin output', () => {
  for (const [clockEnabled, outputMode] of [[false, true], [true, false], [false, false], [undefined, true], [1, true]]) {
    const result = evaluateRegisterWrite({ odrBefore: 0xa55a, writeValue: 0x20, pin: 5, clockEnabled, outputMode })
    assert.equal(result.allowed, false)
    assert.equal(result.after, 0xa55a)
    assert.equal(result.pinAfter, null)
    assert.ok(result.missing.length > 0)
  }
  const result = evaluateRegisterWrite({ odrBefore: 0, writeValue: 0x20, pin: 5, clockEnabled: true, outputMode: true })
  assert.equal(result.allowed, true)
  assert.equal(result.after, 0x20)
  assert.equal(result.pinAfter, 1)
  assert.deepEqual(result.missing, [])
})

test('pure register APIs validate callers rather than masking invalid input', () => {
  for (const value of [-1, UINT32_MAX + 1, 1.5, NaN]) assert.throws(() => splitBsrr(value), RangeError)
  assert.throws(() => applyBsrr(0x10000, 0), RangeError)
  assert.throws(() => bits16(-1), RangeError)
  for (const pin of [-1, 16, 1.5]) assert.throws(() => bsrrForPin(pin, 'set'), RangeError)
  assert.throws(() => bsrrForPin(5, 'toggle'), RangeError)
  assert.throws(() => evaluateRegisterWrite({ odrBefore: 0, writeValue: 0, pin: 16 }), RangeError)
})

async function loadSfc(name, ssr) {
  const filename = fileURLToPath(new URL(name + '.vue', componentDirectory))
  const source = await readFile(filename, 'utf8')
  const { descriptor, errors } = parse(source, { filename })
  assert.deepEqual(errors, [])
  const id = 'data-v-' + name.toLowerCase()
  const compiled = compileScript(descriptor, { id, inlineTemplate: true, templateOptions: { ssr } })
  const styles = descriptor.styles.map(style => {
    const result = compileStyle({ source: style.content, filename, id, scoped: style.scoped })
    assert.deepEqual(result.errors, [])
    return result.code
  }).join('\n')
  return { filename, source, id, code: stripTypeScriptTypes(compiled.content), styles }
}

async function ssrModuleUrl(name) {
  const compiled = await loadSfc(name, true)
  let code = compiled.code
  for (const match of compiled.code.matchAll(/from (['"])([^'"]+)\1/g)) {
    const specifier = match[2]
    const url = specifier.endsWith('.vue')
      ? await ssrModuleUrl(basename(specifier, '.vue'))
      : pathToFileURL(specifier.startsWith('.') ? resolve(dirname(compiled.filename), specifier) : require.resolve(specifier)).href
    code = code.replace(match[0], 'from ' + JSON.stringify(url))
  }
  return 'data:text/javascript;base64,' + Buffer.from(code).toString('base64')
}

for (const [name, root] of [['AddressDecodeLab', 'address-decode-lab'], ['RegisterWriteLab', 'register-write-lab']]) {
  test(name + ' compiles scoped styles and renders on the server without timers or browser globals', async () => {
    const component = await import(await ssrModuleUrl(name))
    const html = await renderToString(createSSRApp(component.default))
    assert.ok(html.includes('data-testid="' + root + '"'))
    assert.ok(html.includes('data-stage="0"'))
    assert.ok(html.includes('data-playing="false"'))
    assert.doesNotMatch(html, /BSRR\s*\|=/)
  })
}

// Opt-in, in-memory browser fixture: no generated files or application entry edits.
// Run with MCU_LABS_BROWSER=1 to check real browser behavior and narrow layouts.
test('browser: independent stages, invalidation, reduced motion and responsive bounds', { skip: process.env.MCU_LABS_BROWSER !== '1', timeout: 60000 }, async () => {
  const vitepressRequire = createRequire(require.resolve('vitepress'))
  const { build } = await import(pathToFileURL(vitepressRequire.resolve('esbuild')).href)
  const { chromium } = await import('@playwright/test')
  const browser = await chromium.launch({ channel: process.env.CI ? 'chromium' : 'chrome', headless: true })
  try {
    const compiled = await Promise.all(['AddressDecodeLab', 'RegisterWriteLab'].map(name => loadSfc(name, false)))
    const scripts = []
    const childStyles = []
    const vuePlugin = { name: 'compile-nested-labs', setup(builder) {
      builder.onLoad({ filter: /\.vue$/ }, async ({ path }) => {
        const child = await loadSfc(basename(path, '.vue'), false)
        childStyles.push(child.styles)
        return { contents: child.code.replace('export default', 'const Component =') + '\nComponent.__scopeId = ' + JSON.stringify(child.id) + ';\nexport default Component;', loader: 'js', resolveDir: dirname(path) }
      })
    } }
    for (const [index, item] of compiled.entries()) {
      const source = item.code.replace('export default', 'const Lab =') + '\nLab.__scopeId = ' + JSON.stringify(item.id) + ';\n' +
        "import { createApp } from 'vue'; globalThis.labApps ||= []; globalThis.labApps.push(createApp(Lab)); globalThis.labApps[" + index + "].mount('#lab-" + index + "');"
      const bundle = await build({ stdin: { contents: source, resolveDir: dirname(item.filename), loader: 'js' }, bundle: true, format: 'iife', write: false, plugins: [vuePlugin], define: { 'process.env.NODE_ENV': '"production"' } })
      scripts.push(bundle.outputFiles[0].text)
    }
    const page = await browser.newPage()
    const errors = []
    page.on('pageerror', error => errors.push(error.message))
    await page.setContent('<div class="vp-doc" style="max-width:820px;margin:auto;padding:24px"><div id="lab-0"></div><div id="lab-1"></div></div>')
    const sharedStyles = await readFile(new URL('../docs/.vitepress/theme/style.css', import.meta.url), 'utf8')
    await page.addStyleTag({ content: 'body{margin:0;font-family:Arial,sans-serif}*{box-sizing:border-box}' + sharedStyles + compiled.map(item => item.styles).concat(childStyles).join('\n') })
    await page.evaluate(() => {
      globalThis.labTimers = new Set()
      globalThis.labMotionListeners = new Set()
      const start = window.setInterval.bind(window)
      const stop = window.clearInterval.bind(window)
      window.setInterval = (...args) => { const id = start(...args); globalThis.labTimers.add(id); return id }
      window.clearInterval = id => { globalThis.labTimers.delete(id); stop(id) }
      const add = MediaQueryList.prototype.addEventListener
      const remove = MediaQueryList.prototype.removeEventListener
      MediaQueryList.prototype.addEventListener = function(type, listener, ...args) {
        if (type === 'change') globalThis.labMotionListeners.add(listener)
        return add.call(this, type, listener, ...args)
      }
      MediaQueryList.prototype.removeEventListener = function(type, listener, ...args) {
        if (type === 'change') globalThis.labMotionListeners.delete(listener)
        return remove.call(this, type, listener, ...args)
      }
    })
    for (const content of scripts) await page.addScriptTag({ content })
    assert.equal(await page.evaluate(() => globalThis.labTimers.size), 0)
    assert.equal(await page.evaluate(() => globalThis.labMotionListeners.size), 2)
    const address = page.getByTestId('address-decode-lab')
    const register = page.getByTestId('register-write-lab')
    const signals = register.getByTestId('bus-signals')
    assert.equal(await address.getAttribute('data-stage'), '0')
    assert.equal(await register.getAttribute('data-stage'), '0')
    for (let i = 0; i < 4; i += 1) await page.getByTestId('address-step').click()
    assert.equal(await page.getByTestId('address-target').textContent(), 'GPIOA_BSRR')
    assert.equal(await page.getByTestId('address-offset').textContent(), '0x00000018')
    assert.equal(await address.locator('.route li[data-match="matched"]').count(), 4)
    await page.getByTestId('address-input').fill('0x40020019')
    assert.equal(await address.getAttribute('data-stage'), '0')
    assert.equal(await page.getByTestId('address-step').isDisabled(), true)
    assert.equal(await page.getByTestId('address-result').getAttribute('data-outcome'), 'blocked')
    await page.getByTestId('address-input').fill('0x10000000')
    for (let i = 0; i < 4; i += 1) await page.getByTestId('address-step').click()
    assert.equal(await page.getByTestId('address-result').getAttribute('data-outcome'), 'unmodeled')
    assert.equal(await address.locator('.route li[data-match="matched"]').count(), 0)
    assert.equal(await register.getAttribute('data-stage'), '0')
    for (const example of ADDRESS_EXAMPLES) {
      await page.getByTestId('address-example').selectOption(formatHex32(example.address))
      assert.equal(await address.getAttribute('data-stage'), '0')
      assert.equal(await page.getByTestId('address-result').getAttribute('data-outcome'), 'pending')
      for (let i = 0; i < 4; i += 1) await page.getByTestId('address-step').click()
      assert.equal(await page.getByTestId('address-target').textContent(), decodeAddress(example.address).target)
    }
    for (let i = 0; i < 3; i += 1) await page.getByTestId('register-step').click()
    assert.equal(await page.getByTestId('register-odr-current').textContent(), '0x0000')
    await page.getByTestId('register-view-signals').click()
    assert.equal(await signals.getAttribute('data-status'), 'valid')
    assert.equal(await signals.getByTestId('signal-word').textContent(), '0x00000020')
    assert.equal(await signals.locator('[data-bit="5"]').getAttribute('data-level'), '1')
    await page.getByTestId('register-step').click()
    assert.equal(await signals.getAttribute('data-status'), 'unspecified')
    assert.equal(await page.getByTestId('register-odr-after').textContent(), '0x0020')
    assert.equal(await page.getByTestId('register-pin-state').textContent(), '待本次引脚阶段')
    await page.getByTestId('register-step').click()
    assert.equal(await page.getByTestId('register-pin-state').textContent(), '1')
    await page.getByTestId('register-clear').click()
    assert.equal(await register.getAttribute('data-stage'), '0')
    assert.equal(await page.getByTestId('register-write-value').inputValue(), '0x00200000')
    assert.equal(await page.getByTestId('register-odr-before').textContent(), '0x0020')
    assert.equal(await page.getByTestId('register-odr-after').textContent(), '待写入')
    for (let i = 0; i < 5; i += 1) await page.getByTestId('register-step').click()
    assert.equal(await page.getByTestId('register-pin-state').textContent(), '0')
    await page.getByTestId('register-clock').uncheck()
    assert.equal(await page.getByTestId('register-step').isDisabled(), true)
    assert.match(await page.getByTestId('register-result').textContent(), /前置条件不足/)
    assert.equal(await signals.getAttribute('data-status'), 'unspecified')
    assert.equal(await signals.locator('[data-level="unknown"][data-bit]').count(), 32)
    await page.getByTestId('register-clock').check()
    await page.getByTestId('register-output-mode').uncheck()
    assert.equal(await page.getByTestId('register-step').isDisabled(), true)
    assert.equal(await page.getByTestId('register-odr-current').textContent(), '0x0000')
    assert.match(await page.getByTestId('register-result').textContent(), /输出模式/)
    await page.getByTestId('register-output-mode').check()
    await page.getByTestId('register-write-value').fill('0x80008000')
    await page.getByTestId('register-pin').selectOption('15')
    for (let i = 0; i < 5; i += 1) await page.getByTestId('register-step').click()
    assert.equal(await page.getByTestId('register-odr-after').textContent(), '0x8000')
    assert.equal(await page.getByTestId('register-pin-state').textContent(), '1')
    assert.match(await page.getByTestId('register-code').textContent(), /0x80008000u;/)
    await page.getByTestId('register-view-bits').click()
    for (const width of [320, 390, 1440]) {
      await page.setViewportSize({ width, height: 1000 })
      assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > window.innerWidth), false, 'page overflow at ' + width)
      const overflow = await page.locator('[data-testid$="-lab"] input, [data-testid$="-lab"] select, [data-testid$="-lab"] button, [data-testid$="-lab"] output, [data-testid$="-lab"] .bit').evaluateAll(elements => elements.filter(element => element.scrollWidth > element.clientWidth + 1).map(element => element.outerHTML))
      assert.deepEqual(overflow, [], 'control overflow at ' + width)
      const columns = await page.getByTestId('register-set-bits').evaluate(element => getComputedStyle(element).gridTemplateColumns.split(' ').length)
      assert.equal(columns, width < 650 ? 8 : 16)
      assert.ok((await page.screenshot()).byteLength > 10000)
      await page.getByTestId('address-input').fill('0x40020018')
      await page.getByTestId('address-reset').click()
      await page.getByTestId('register-reset').click()
      const addressHeight = (await address.boundingBox()).height
      const registerHeight = (await register.boundingBox()).height
      await page.getByTestId('register-view-signals').click()
      assert.equal((await register.boundingBox()).height, registerHeight, 'signal view height at ' + width)
      for (let step = 0; step < 5; step += 1) {
        if (step < 4) await page.getByTestId('address-step').click()
        await page.getByTestId('register-step').click()
        assert.equal((await address.boundingBox()).height, addressHeight, 'address layout shift at ' + width + ', step ' + step)
        assert.equal((await register.boundingBox()).height, registerHeight, 'register layout shift at ' + width + ', step ' + step)
        assert.equal(await signals.getAttribute('data-status'), step === 2 ? 'valid' : 'unspecified')
        if (step === 2) {
          assert.deepEqual(await signals.locator('[data-level="1"][data-bit]').evaluateAll(bits => bits.map(bit => Number(bit.dataset.bit))), [31, 15])
          if (process.env.MCU_SIGNAL_ARTIFACTS) await register.locator('.command-visual').screenshot({ path: process.env.MCU_SIGNAL_ARTIFACTS + '/bsrr-' + width + '.png', animations: 'disabled' })
        }
      }
      for (const invalid of ['-1', '0x100000000', '0xZZ']) {
        await page.getByTestId('address-input').fill(invalid)
        await page.getByTestId('register-write-value').fill(invalid)
        assert.equal(await page.getByTestId('address-step').isDisabled(), true)
        assert.equal(await page.getByTestId('register-step').isDisabled(), true)
        assert.equal((await address.boundingBox()).height, addressHeight, 'address validation layout shift at ' + width)
        assert.equal((await register.boundingBox()).height, registerHeight, 'register validation layout shift at ' + width)
      }
      await page.getByTestId('address-input').fill('0x40020018')
      await page.getByTestId('register-write-value').fill('0x80008000')
      await page.getByTestId('register-view-bits').click()
    }
    await page.getByTestId('register-reset').click()
    await page.getByTestId('register-play').click()
    assert.equal(await register.getAttribute('data-playing'), 'true')
    await page.getByTestId('register-write-value').fill('0x20')
    assert.equal(await register.getAttribute('data-playing'), 'false')
    assert.equal(await register.getAttribute('data-stage'), '0')
    await page.getByTestId('register-view-signals').click()
    for (let i = 0; i < 3; i++) await page.getByTestId('register-step').click()
    assert.equal(await signals.getAttribute('data-status'), 'valid')
    await page.getByTestId('register-clock').uncheck()
    assert.equal(await signals.getAttribute('data-status'), 'unspecified')
    assert.equal(await signals.locator('.level-line').count(), 0)
    assert.equal(await signals.locator('.window-sweep').count(), 0)
    await page.getByTestId('register-clock').check()
    await page.emulateMedia({ reducedMotion: 'reduce' })
    await page.getByTestId('register-play').click()
    assert.equal(await register.getAttribute('data-playing'), 'false')
    assert.equal(await register.getAttribute('data-stage'), '1')
    await page.getByTestId('register-reset').click()
    await page.emulateMedia({ reducedMotion: 'no-preference' })
    await page.getByTestId('register-play').click()
    await page.waitForFunction(() => document.querySelector('[data-testid="register-write-lab"]').dataset.stage === '1')
    await page.getByTestId('register-play').click()
    assert.equal(await register.getAttribute('data-playing'), 'false')
    await page.getByTestId('address-reset').click()
    await page.getByTestId('address-play').click()
    await page.getByTestId('register-play').click()
    assert.equal(await page.evaluate(() => globalThis.labTimers.size), 2)
    await page.evaluate(() => globalThis.labApps.forEach(app => app.unmount()))
    assert.equal(await page.evaluate(() => globalThis.labTimers.size), 0)
    assert.equal(await page.evaluate(() => globalThis.labMotionListeners.size), 0)
    assert.deepEqual(errors, [])
  } finally {
    await browser.close()
  }
})
