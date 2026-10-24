import { test, expect, type Locator, type Page } from '@playwright/test';

// Isolate the diagram from fixed page navigation during component screenshots.
const componentShot = { animations: 'disabled' as const, style: '.VPNav, .VPLocalNav { visibility: hidden !important; }' };

async function openPage(page: Page, path: string) {
  const response = await page.goto(path);
  expect(response?.status()).toBe(200);
  await page.waitForFunction(() => Boolean((document.querySelector('#app') as Element & { __vue_app__?: unknown })?.__vue_app__));
}
async function steps(root: Locator, button: string, count: number) {
  for (let i = 0; i < count; i++) await root.getByTestId(button).click();
}
async function layout(page: Page, roots: Locator[]) {
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1)).toBe(true);
  for (const root of roots) {
    const result = await root.evaluate(element => {
      const frame = element.getBoundingClientRect();
      const controls = [...element.querySelectorAll('button,input,select')].map(e => ({ name: e.getAttribute('data-testid') || e.tagName, rect: e.getBoundingClientRect() })).filter(x => x.rect.width && x.rect.height);
      const outside = controls.filter(x => x.rect.left < frame.left - 1 || x.rect.right > frame.right + 1).map(x => x.name);
      const collisions: string[] = [];
      for (let i = 0; i < controls.length; i++) for (let j = i + 1; j < controls.length; j++) {
        const a = controls[i].rect, b = controls[j].rect;
        if (Math.min(a.right, b.right) - Math.max(a.left, b.left) > 2 && Math.min(a.bottom, b.bottom) - Math.max(a.top, b.top) > 2) collisions.push(controls[i].name + ':' + controls[j].name);
      }
      return { overflow: element.scrollWidth > element.clientWidth + 1, outside, collisions };
    });
    expect(result).toEqual({ overflow: false, outside: [], collisions: [] });
  }
}

test('principle links leave the pending list and reach both articles', async ({ page }) => {
  await openPage(page, 'lessons/2-1');
  const architecture = page.locator('.vp-doc a[href$="/notes/mcu-architecture"]').first();
  await expect(architecture).not.toHaveAttribute('data-pending', 'true');
  await architecture.click();
  await expect(page.getByTestId('mcu-bus-lab')).toBeVisible();
  await expect(page.getByTestId('flash-execution-lab')).toBeAttached();
  await openPage(page, 'lessons/2-1');
  await page.locator('.vp-doc a[href$="/notes/memory-mapped-io"]').first().click();
  await expect(page.getByTestId('address-decode-lab')).toBeVisible();
  await expect(page.getByTestId('register-write-lab')).toBeAttached();
  await openPage(page, 'topics/');
  await expect(page.locator('#topic-a842ebf1eb')).toBeAttached();
  await expect(page.locator('#topic-5566ad0f25')).toBeAttached();
  await expect(page.locator('.vp-doc a[href$="/notes/mcu-architecture"]')).toBeVisible();
});

test('architecture paths and Flash startup preserve distinct memory states', async ({ page }, info) => {
  const errors: string[] = [];
  page.on('pageerror', e => errors.push(e.message));
  await openPage(page, 'notes/mcu-architecture');
  const bus = page.getByTestId('mcu-bus-lab');
  const flash = page.getByTestId('flash-execution-lab');
  await expect(bus).toHaveAttribute('data-playing', 'false');
  await bus.getByTestId('mcu-mode-sram').click();
  await steps(bus, 'mcu-bus-next', 4);
  await expect(bus.getByTestId('mcu-bus-r0')).toHaveText('42');
  await bus.getByTestId('mcu-mode-flash').click();
  await expect(bus).toHaveAttribute('data-step', '0');
  await expect(bus.getByTestId('mcu-icode-path')).toContainText('总线矩阵');
  await steps(bus, 'mcu-bus-next', 4);
  await expect(bus.getByTestId('mcu-bus-address')).toHaveText('0x08000400');
  await bus.getByTestId('mcu-mode-gpio').click();
  await steps(bus, 'mcu-bus-next', 4);
  await expect(bus.getByTestId('mcu-bus-pa5')).toContainText('高');
  await expect(bus.getByTestId('mcu-bus-route')).toContainText('总线矩阵');
  await expect(flash.getByTestId('flash-ram-counter')).toContainText('未知');
  const initial = await flash.screenshot({ ...componentShot });
  await steps(flash, 'flash-next', 10);
  await expect(flash.getByTestId('flash-ram-counter')).toHaveText('8');
  await expect(flash.getByTestId('flash-counter-live')).toHaveText('8');
  await expect(flash.getByTestId('flash-ram-events')).toHaveText('0');
  await expect(flash.getByTestId('flash-initial-value')).toHaveText('7');
  await expect(flash.getByTestId('flash-reset-vector')).toHaveText('0x08000201');
  await expect(flash.getByTestId('flash-pc')).toHaveText('0x08000400');
  const finished = await flash.screenshot({ path: info.outputPath('flash-execution.png'), ...componentShot });
  expect(initial.equals(finished)).toBe(false);
  await bus.screenshot({ path: info.outputPath('mcu-bus.png'), ...componentShot });
  await layout(page, [bus, flash]);
  await flash.getByTestId('flash-reset').click();
  await expect(flash.getByTestId('flash-ram-counter')).toContainText('未知');
  await bus.getByTestId('mcu-bus-reset').click();
  await bus.getByTestId('mcu-bus-play').click();
  await expect.poll(async () => Number(await bus.getAttribute('data-step'))).toBeGreaterThan(0);
  await bus.getByTestId('mcu-bus-play').click();
  await expect(bus).toHaveAttribute('data-playing', 'false');
  expect(errors).toEqual([]);
});

test('MMIO decode and BSRR commands agree with the article', async ({ page }, info) => {
  const errors: string[] = [];
  page.on('pageerror', e => errors.push(e.message));
  await openPage(page, 'notes/memory-mapped-io');
  const address = page.getByTestId('address-decode-lab');
  const register = page.getByTestId('register-write-lab');
  await address.getByTestId('address-input').fill('0x40023830');
  await steps(address, 'address-step', 4);
  await expect(address.getByTestId('address-target')).toContainText('AHB1ENR');
  await expect(address.getByTestId('address-base')).toHaveText('RCC · 0x40023800');
  await expect(address.getByTestId('address-offset')).toHaveText(/0x0*30/);
  await address.screenshot({ path: info.outputPath('address-decode.png'), ...componentShot });
  await steps(register, 'register-step', 5);
  await expect(register.getByTestId('register-odr-current')).toHaveText('0x0020');
  await expect(register.getByTestId('register-pin-state')).toHaveText('1');
  await register.getByTestId('register-clear').click();
  await steps(register, 'register-step', 5);
  await expect(register.getByTestId('register-odr-current')).toHaveText('0x0000');
  await register.getByTestId('register-write-value').fill('0x00200020');
  await steps(register, 'register-step', 5);
  await expect(register.getByTestId('register-odr-current')).toHaveText('0x0020');
  await register.screenshot({ path: info.outputPath('register-write.png'), ...componentShot });
  await address.getByTestId('address-input').fill('0xDEADBEEF');
  await steps(address, 'address-step', 4);
  await expect(address.getByTestId('address-result')).toHaveAttribute('data-outcome', 'unmodeled');
  await address.getByTestId('address-input').fill('0x100000000');
  await expect(address.getByTestId('address-step')).toBeDisabled();
  await layout(page, [address, register]);
  expect(errors).toEqual([]);
});

test('register prerequisites, editing and reduced motion remain explicit', async ({ page }, info) => {
  await openPage(page, 'notes/memory-mapped-io');
  const address = page.getByTestId('address-decode-lab');
  const register = page.getByTestId('register-write-lab');
  await register.getByTestId('register-clock').uncheck();
  if (await register.getByTestId('register-step').isEnabled()) await steps(register, 'register-step', 5);
  await expect(register.getByTestId('register-result')).toHaveAttribute('data-outcome', 'blocked');
  await expect(register.getByTestId('register-odr-current')).toHaveText('0x0000');
  await register.getByTestId('register-clock').check();
  await register.getByTestId('register-play').click();
  await expect(register).toHaveAttribute('data-playing', 'true');
  await register.getByTestId('register-write-value').fill('0x20');
  await expect(register).toHaveAttribute('data-playing', 'false');
  await expect(register).toHaveAttribute('data-stage', '0');
  await page.emulateMedia({ reducedMotion: 'reduce', colorScheme: 'dark' });
  await page.evaluate(() => document.documentElement.classList.add('dark'));
  await register.getByTestId('register-play').click();
  await expect(register).toHaveAttribute('data-playing', 'false');
  await expect(register).toHaveAttribute('data-stage', '1');
  await steps(register, 'register-step', 4);
  await expect(register.getByTestId('register-odr-current')).toHaveText('0x0020');
  await register.screenshot({ path: info.outputPath('register-dark.png'), ...componentShot });
  await address.screenshot({ path: info.outputPath('address-dark.png'), ...componentShot });
  await layout(page, [address, register]);
});
