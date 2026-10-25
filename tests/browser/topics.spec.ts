import { test, expect } from '@playwright/test';
import { readFileSync } from 'node:fs';

const { topics } = JSON.parse(readFileSync(new URL('../../docs/.vitepress/topic-manifest.json', import.meta.url), 'utf8'));
const groups = [...new Set(topics.map((topic: { group: string }) => topic.group))];

test('all 102 articles are discoverable without pending placeholders', async ({ page }) => {
  expect((await page.goto('topics/'))?.status()).toBe(200);
  const article = page.locator('.vp-doc');
  await expect(article.locator('h1')).toHaveText('原理与拓展');
  await expect(article.locator('a[href*="/notes/"]')).toHaveCount(102);
  await expect(article.locator('[data-pending="true"]')).toHaveCount(0);
  for (const topic of topics) {
    await expect(article.locator('a[href="/from-c-to-mcu' + topic.link + '"]')).toHaveCount(1);
    if (topic.anchor) await expect(article.locator('#' + topic.anchor)).toHaveCount(1);
  }
  await article.locator('a[href="/from-c-to-mcu/notes/git-workflow"]').click();
  await expect(page).toHaveURL(/\/notes\/git-workflow$/);
});

test('new topic is searchable and opens its actual article', async ({ page }) => {
  await page.goto('./');
  await page.getByRole('button', { name: '搜索课程', exact: true }).click();
  const input = page.locator('input[type=search]');
  await expect(input).toBeVisible();
  await input.fill('CAN位定时与采样点');
  const result = page.locator('.VPLocalSearchBox a[href*="/notes/can-bit-timing"]').first();
  await expect(result).toBeVisible();
  await result.click();
  await expect(page).toHaveURL(/\/notes\/can-bit-timing(?:#.*)?$/);
  await expect(page.locator('.vp-doc h1')).toContainText('CAN');
});

for (const group of groups) {
  test('completed articles render, reveal answers and retain local graphs: ' + group, async ({ page }, info) => {
    test.setTimeout(180000);
    const errors: string[] = [];
    page.on('pageerror', error => errors.push(error.message));
    for (const topic of topics.filter((item: { group: string }) => item.group === group)) {
      await test.step(topic.key, async () => {
        expect((await page.goto(topic.link.slice(1)))?.status()).toBe(200);
        const article = page.locator('.vp-doc');
        await expect(article.locator('h1')).toBeVisible();
        await expect(article.locator('h2,h3').first()).toBeVisible();
        await expect(article.locator('[data-pending="true"]')).toHaveCount(0);
        const graph = page.getByTestId(page.viewportSize()!.width < 1280 ? 'local-graph-mobile' : 'local-graph');
        await expect(graph).toHaveAttribute('data-current', topic.link);
        const answer = article.locator('details').filter({ has: page.locator('summary', { hasText: /答案|解析|分析|推演|结果|揭晓|检查|对照|参考|验证/ }) }).first();
        await expect(answer).toBeAttached();
        await answer.locator('summary').click();
        await expect(answer).toHaveAttribute('open', '');
        expect(await article.locator('a[href*="/lessons/"]').count()).toBeGreaterThan(0);
        const overflow = await page.evaluate(() => document.documentElement.scrollWidth - innerWidth);
        expect(overflow, topic.path + ' document overflow').toBeLessThanOrEqual(1);
        if (['/notes/git-workflow', '/notes/can-bit-timing', '/notes/embedded-cpp'].includes(topic.link)) {
          await page.evaluate(() => window.scrollTo(0, 0));
          await page.screenshot({ path: info.outputPath(topic.link.split('/').at(-1) + '.png'), animations: 'disabled' });
        }
      });
    }
    expect(errors).toEqual([]);
  });
}
