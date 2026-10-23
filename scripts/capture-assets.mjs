import { chromium } from '@playwright/test';
import fs from 'node:fs/promises';
await fs.mkdir('docs/public',{recursive:true});
const browser=await chromium.launch({channel:'chrome'});
for(const theme of ['light','dark']){
  const context=await browser.newContext({viewport:{width:1280,height:900},colorScheme:theme});
  const page=await context.newPage();await page.goto('http://127.0.0.1:4318/from-c-to-mcu/labs/#pwm');
  const canvas=page.getByTestId('pwm-lab').locator('canvas');await canvas.scrollIntoViewIfNeeded();await page.waitForFunction(()=>document.querySelector('[data-testid="pwm-lab"] canvas')?.width>0);
  await canvas.screenshot({path:'docs/public/pwm-'+theme+'.png',omitBackground:true});await context.close();
}
await browser.close();console.log('Captured original PWM waveform assets.');
