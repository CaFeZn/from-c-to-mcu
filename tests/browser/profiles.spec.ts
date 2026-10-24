import { test, expect } from '@playwright/test';

test('profiles stay connected across setup and debugging lessons', async ({ page }, info) => {
  const errors: string[] = [];
  page.on('pageerror', error => errors.push(error.message));
  const noOverflow = async () => expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1)).toBeTruthy();

  await page.goto('lessons/1-0');
  await expect(page.getByRole('heading', { name: /1\. 安装 VS Code，建立 C-PC Profile/ })).toBeVisible();
  await expect(page.locator('.vp-doc')).toContainText('Profiles: Switch Profile');
  await expect(page.locator('.vp-doc')).toContainText('Folders & Workspaces');
  await noOverflow();
  await page.getByRole('heading', { name: /1\. 安装 VS Code，建立 C-PC Profile/ }).scrollIntoViewIfNeeded();
  await page.screenshot({ path: info.outputPath('profile-pc.png') });

  await page.locator('.vp-doc a[href*="/lessons/2-2#"]').first().click();
  await expect(page.getByRole('heading', { name: /先建立 STM32 Profile/ })).toBeVisible();
  await expect(page.locator('.vp-doc')).toContainText('Profile 不是 CMake 的配置预设');
  await expect(page.locator('.vp-doc')).toContainText('不要在已经打开');
  await noOverflow();
  await page.getByRole('heading', { name: /先建立 STM32 Profile/ }).scrollIntoViewIfNeeded();
  await page.screenshot({ path: info.outputPath('profile-stm32.png') });

  await page.goto('lessons/3-2');
  await expect(page.locator('.vp-doc')).toContainText('确认当前窗口使用 STM32 Profile');
  await page.getByRole('link', { name: '2.2 的 Profile 设置', exact: true }).click();
  await expect(page.getByRole('heading', { name: /先建立 STM32 Profile/ })).toBeVisible();

  await page.goto('notes/editor-setup');
  await expect(page.locator('.vp-doc')).toContainText('本文的 c-lab 用 C-PC，板级工程用 STM32');
  await noOverflow();
  expect(errors).toEqual([]);
});
