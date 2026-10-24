import { test, expect, type Page, type Locator } from '@playwright/test';
const article = 'notes/mcu-architecture';
const current = '/notes/mcu-architecture';
const target = '/notes/memory-mapped-io';
async function open(page: Page) {
  expect((await page.goto(article))?.status()).toBe(200);
  await page.waitForFunction(() => Boolean((document.querySelector('#app') as Element & { __vue_app__?: unknown })?.__vue_app__));
}
async function localView(page: Page, expectedId = current) {
  if (page.viewportSize()!.width < 1280) {
    const launcher = page.getByTestId('local-graph-mobile');
    await expect(launcher).toBeVisible();
    await expect(launcher).toHaveAttribute('data-current', expectedId);
    await launcher.getByTestId('local-graph-open').click();
    const dialog = page.locator('dialog[open][data-testid="graph-dialog"]');
    await expect(dialog.getByTestId('graph-view')).toHaveAttribute('data-ready', 'true');
    return { root: dialog, view: dialog.getByTestId('graph-view') };
  }
  const root = page.getByTestId('local-graph');
  await expect(root).toBeVisible();
  await expect(root).toHaveAttribute('data-current', expectedId);
  await expect(root.getByTestId('graph-view')).toHaveAttribute('data-ready', 'true');
  return { root, view: root.getByTestId('graph-view') };
}
async function ids(view: Locator) { return (await view.locator('.graph-node').evaluateAll(nodes => nodes.map(node => node.getAttribute('data-page-id')!))).sort(); }

test('current-file graph has real neighbors and follows node navigation', async ({ page }) => {
  const errors: string[] = [];
  page.on('pageerror', e => errors.push(e.message));
  await open(page);
  const { view } = await localView(page);
  await expect(view).toHaveAttribute('data-current', current);
  const nodes = await ids(view);
  expect(nodes).toContain(current);
  expect(nodes).toContain(target);
  expect(nodes.length).toBeGreaterThan(2);
  expect(nodes.length).toBeLessThan(52);
  expect(await view.locator('.graph-node').evaluateAll(links => links.every(link => (link as SVGElement & { pathname: string }).pathname === new URL(link.getAttribute('href')!, location.href).pathname))).toBe(true);
  await expect(view.locator('.graph-node[aria-current="page"]')).toHaveCount(1);
  const link = view.locator('[data-page-id="' + target + '"]');
  await expect(link).toHaveAttribute('href', '/from-c-to-mcu' + target);
  await link.locator('.graph-dot').click();
  await expect(page).toHaveURL(/\/notes\/memory-mapped-io$/);
  const after = await localView(page, target);
  await expect(after.view).toHaveAttribute('data-current', target);
  expect(await ids(after.view)).toContain(current);
  expect(errors).toEqual([]);
});

test('expanded graph stays local, supports zoom drag reset and Escape', async ({ page }) => {
  await open(page);
  const initialOverflow = await page.evaluate(() => document.body.style.overflow);
  const initial = await localView(page);
  const expected = await ids(initial.view);
  if (page.viewportSize()!.width >= 1280) await initial.root.getByTestId('local-graph-open').click();
  const dialog = page.locator('dialog[open][data-testid="graph-dialog"]');
  const view = dialog.getByTestId('graph-view');
  await expect(view).toHaveAttribute('data-ready', 'true');
  expect(await ids(view)).toEqual(expected);
  await dialog.getByRole('button', { name: '放大关系图', exact: true }).click();
  await expect(view).toHaveAttribute('data-scale', '1.25');
  await dialog.getByRole('button', { name: '重置关系图视图', exact: true }).click();
  await expect(view).toHaveAttribute('data-scale', '1.00');
  const center = view.locator('.graph-node.current .graph-dot');
  const beforeX = Number(await center.getAttribute('cx'));
  const box = (await center.boundingBox())!;
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down();
  await page.mouse.move(box.x + box.width / 2 + 28, box.y + box.height / 2 + 18, { steps: 8 });
  await page.mouse.up();
  expect(Math.abs(Number(await center.getAttribute('cx')) - beforeX)).toBeGreaterThan(8);
  await expect(page).toHaveURL(/\/notes\/mcu-architecture$/);
  await page.keyboard.press('Escape');
  await expect(dialog).toHaveCount(0);
  expect(await page.evaluate(() => document.body.style.overflow)).toBe(initialOverflow);
});

test('graph sits below the outline and remains usable in dark narrow layouts', async ({ page }, info) => {
  await open(page);
  const local = await localView(page);
  if (page.viewportSize()!.width >= 1280) {
    const outline = (await page.locator('.VPDocAsideOutline').boundingBox())!;
    const graph = (await local.root.boundingBox())!;
    expect(graph.y).toBeGreaterThanOrEqual(outline.y + outline.height - 2);
    await page.screenshot({ path: info.outputPath('article-with-local-graph.png'), animations: 'disabled' });
  }
  const positions = await local.view.locator('.graph-dot').evaluateAll(circles => circles.map(circle => ({ x: Number(circle.getAttribute('cx')), y: Number(circle.getAttribute('cy')) })));
  expect(positions.every(p => Number.isFinite(p.x) && Number.isFinite(p.y))).toBe(true);
  await expect(local.view.locator('.graph-edges line').first()).toBeAttached();
  const labels = await local.view.locator('text').evaluateAll(items => items.map(item => { const r = item.getBoundingClientRect(); return { left: r.left, right: r.right, top: r.top, bottom: r.bottom }; }));
  for (let i = 0; i < labels.length; i++) for (let j = i + 1; j < labels.length; j++) {
    const a = labels[i], b = labels[j];
    expect(Math.min(a.right, b.right) - Math.max(a.left, b.left) > 1 && Math.min(a.bottom, b.bottom) - Math.max(a.top, b.top) > 1).toBe(false);
  }
  const light = await local.root.screenshot({ path: info.outputPath('local-graph-light.png'), animations: 'disabled' });
  expect(light.byteLength).toBeGreaterThan(3000);
  await page.emulateMedia({ reducedMotion: 'reduce', colorScheme: 'dark' });
  await page.evaluate(() => document.documentElement.classList.add('dark'));
  const dark = await local.root.screenshot({ path: info.outputPath('local-graph-dark.png'), animations: 'disabled' });
  expect(light.equals(dark)).toBe(false);
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1)).toBe(true);
  const controls = await local.root.locator('button').evaluateAll(buttons => buttons.filter(button => button.getBoundingClientRect().width).map(button => ({ width: button.getBoundingClientRect().width, left: button.getBoundingClientRect().left, right: button.getBoundingClientRect().right })));
  expect(controls.every(b => b.width >= 24 && b.left >= 0 && b.right <= page.viewportSize()!.width)).toBe(true);
});

test('graph keeps focus and cleans up an interrupted pan', async ({ page }) => {
  await page.addInitScript(() => {
    const tracked = new Set(['mousemove', 'mouseup', 'dragstart', 'selectstart']);
    const sets = new Map<string, Set<unknown>>();
    const add = window.addEventListener.bind(window), remove = window.removeEventListener.bind(window);
    window.addEventListener = ((type: string, listener: unknown, options: unknown) => { if (tracked.has(type)) { if (!sets.has(type)) sets.set(type, new Set()); sets.get(type)!.add(listener); } return add(type, listener as EventListener, options as boolean); }) as typeof window.addEventListener;
    window.removeEventListener = ((type: string, listener: unknown, options: unknown) => { sets.get(type)?.delete(listener); return remove(type, listener as EventListener, options as boolean); }) as typeof window.removeEventListener;
    (window as any).__graphListeners = () => [...sets.values()].reduce((count, set) => count + set.size, 0);
  });
  await open(page);
  const first = await localView(page);
  if (page.viewportSize()!.width >= 1280) await first.root.getByTestId('local-graph-open').click();
  const dialog = page.locator('dialog[open][data-testid="graph-dialog"]');
  const view = dialog.getByTestId('graph-view');
  await expect(view).toHaveAttribute('data-ready', 'true');
  const nodes = view.locator('.graph-node');
  for (let i = 0; i < await nodes.count(); i++) {
    await nodes.nth(i).focus();
    const overlaps = await view.locator('text').evaluateAll(elements => {
      const r = elements.map(e => e.getBoundingClientRect());
      return r.some((a, i) => r.slice(i + 1).some(b => Math.min(a.right, b.right) - Math.max(a.left, b.left) > 1 && Math.min(a.bottom, b.bottom) - Math.max(a.top, b.top) > 1));
    });
    expect(overlaps).toBe(false);
  }
  await dialog.getByRole('button', { name: '关闭关系图', exact: true }).focus();
  for (let i = 0; i < 24; i++) {
    await page.keyboard.press('Tab');
    expect(await page.evaluate(() => Boolean(document.activeElement?.closest('dialog[open]')))).toBe(true);
  }
  await dialog.getByRole('button', { name: '放大关系图', exact: true }).click();
  const center = view.locator('.graph-node.current .graph-dot');
  const x = Number(await center.getAttribute('cx'));
  const dot = (await center.boundingBox())!;
  await page.mouse.move(dot.x + dot.width / 2, dot.y + dot.height / 2);
  await page.mouse.down();
  await page.mouse.move(dot.x + dot.width / 2 + 30, dot.y + dot.height / 2 + 15, { steps: 6 });
  await page.mouse.up();
  expect(Math.abs(Number(await center.getAttribute('cx')) - x)).toBeGreaterThan(6);
  const before = await page.evaluate(() => (window as any).__graphListeners());
  const box = (await view.boundingBox())!;
  await page.mouse.move(box.x + 2, box.y + 2);
  await page.mouse.down();
  await page.mouse.move(box.x + 22, box.y + 22, { steps: 4 });
  expect(await page.evaluate(() => (window as any).__graphListeners())).toBeGreaterThan(before);
  await page.keyboard.press('Escape');
  await page.mouse.up();
  await expect(dialog).toHaveCount(0);
  expect(await page.evaluate(() => (window as any).__graphListeners())).toBe(before);
  const opener = page.viewportSize()!.width >= 1280 ? page.getByTestId('local-graph').getByTestId('local-graph-open') : page.getByTestId('local-graph-mobile').getByTestId('local-graph-open');
  await expect(opener).toBeFocused();
});
