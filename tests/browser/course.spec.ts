import { test, expect } from '@playwright/test';
const errors = new WeakMap();
test.beforeEach(async({page})=>{const list=[];errors.set(page,list);page.on('pageerror',e=>list.push(e.message));});
test.afterEach(async({page})=>{expect(errors.get(page)).toEqual([]);});
async function noOverflow(page){expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1)).toBeTruthy()}
test('course navigation, lesson and folded answers',async({page},info)=>{
  await page.goto('./');await expect(page.getByRole('heading',{name:'从 C 到单片机',exact:true})).toBeVisible();
  await noOverflow(page);await page.screenshot({path:info.outputPath('home.png'),fullPage:true});
  await page.locator('.lesson-grid a').first().click();await expect(page.locator('h1')).toContainText('跑通 hello');
  await expect(page.getByTestId('build-lab')).toBeVisible();
  const detail=page.locator('details.course-callout').first();await expect(detail).not.toHaveAttribute('open','');await detail.locator('summary').click();await expect(detail).toHaveAttribute('open','');
  await expect(page.locator('.vp-doc')).not.toContainText('[[1.1_Hello_World]]');await noOverflow(page);
  await page.getByRole('checkbox',{name:'已完成本节'}).check();await page.reload();await expect(page.getByRole('checkbox',{name:'已完成本节'})).toBeChecked();
});
test('build pipeline invalidates edited source',async({page},info)=>{
  await page.goto('labs/');const lab=page.getByTestId('build-lab');await lab.scrollIntoViewIfNeeded();
  const input=lab.locator('input[type=text]');await input.fill('Hello browser');
  for(let i=0;i<4;i++)await lab.getByRole('button',{name:'下一步',exact:true}).click();
  await expect(lab.getByTestId('run-greeting')).toHaveText('Hello browser');
  await input.fill('Fresh build');await expect(lab.getByTestId('run-greeting')).toHaveCount(0);await expect(lab.getByTestId('build-status-badge')).toHaveText('未开始');
  await lab.getByRole('button',{name:'播放',exact:true}).click();await expect(lab.getByTestId('run-greeting')).toHaveText('Fresh build');
  await lab.getByRole('button',{name:'重置',exact:true}).click();await expect(lab.getByTestId('run-greeting')).toHaveCount(0);
  await noOverflow(page);await lab.screenshot({path:info.outputPath('build.png')});
});
test('uint8 bit operations and wraparound',async({page},info)=>{
  await page.goto('labs/#bits');const lab=page.getByTestId('bits-lab');await lab.scrollIntoViewIfNeeded();
  for(let i=0;i<8;i++)await lab.getByTestId('bit-'+i).click();
  await expect(lab.getByTestId('bits-decimal')).toHaveText('255');await expect(lab.getByTestId('bits-hex')).toHaveText('0xFF');
  await lab.getByRole('button',{name:'下一步',exact:true}).click();await expect(lab.getByTestId('bits-decimal')).toHaveText('0');
  await lab.locator('select').selectOption('3');await lab.getByRole('button',{name:'置位',exact:true}).click();await expect(lab.getByTestId('bits-decimal')).toHaveText('8');
  await lab.getByRole('button',{name:'翻转',exact:true}).click();await expect(lab.getByTestId('bits-decimal')).toHaveText('0');
  await lab.getByRole('button',{name:'播放',exact:true}).click();await expect.poll(async()=>Number(await lab.getByTestId('bits-decimal').textContent())).toBeGreaterThan(0);
  await lab.getByRole('button',{name:'暂停',exact:true}).click();const value=await lab.getByTestId('bits-decimal').textContent();await page.waitForTimeout(750);await expect(lab.getByTestId('bits-decimal')).toHaveText(value!);
  await noOverflow(page);await lab.screenshot({path:info.outputPath('bits.png')});
});
test('PWM responds to controls and canvas moves',async({page},info)=>{
  await page.goto('labs/#pwm');const lab=page.getByTestId('pwm-lab');await lab.scrollIntoViewIfNeeded();
  const setRange=async(label,value)=>lab.getByRole('slider',{name:label,exact:true}).evaluate((el,value)=>{el.value=String(value);el.dispatchEvent(new Event('input',{bubbles:true}))},value);
  await setRange('占空比',0);await expect(lab.getByTestId('pwm-output')).toHaveText('低电平');
  await setRange('占空比',100);await expect(lab.getByTestId('pwm-output')).toHaveText('高电平');
  await setRange('占空比',75);await setRange('演示频率',5);await expect(lab.getByTestId('pwm-duty')).toHaveText('75%');await expect(lab.locator('.pwm-readings')).toContainText('200ms');await expect(lab.locator('.pwm-readings')).toContainText('150ms');
  const pixels=()=>lab.locator('canvas').evaluate(canvas=>{const data=canvas.getContext('2d').getImageData(0,0,canvas.width,canvas.height).data;let opaque=0,hash=0;for(let i=0;i<data.length;i+=4){if(data[i+3])opaque++;hash=(hash+data[i]*(i+1)+data[i+1]*17+data[i+2]*31)>>>0}return {opaque,hash}});
  const first=await pixels();expect(first.opaque).toBeGreaterThan(1000);await lab.getByRole('button',{name:'播放',exact:true}).click();await expect.poll(async()=>(await pixels()).hash).not.toBe(first.hash);await lab.getByRole('button',{name:'暂停',exact:true}).click();
  await noOverflow(page);await lab.screenshot({path:info.outputPath('pwm.png')});
});
test('interrupt flag and main-loop handling follow the trace',async({page},info)=>{
  await page.goto('labs/#interrupt');const lab=page.getByTestId('interrupt-lab');await lab.scrollIntoViewIfNeeded();
  await lab.getByRole('button',{name:'触发中断',exact:true}).click();await expect(lab.getByTestId('irq-lane')).toHaveText('外部事件');
  await lab.getByRole('button',{name:'下一步',exact:true}).click();await lab.getByRole('button',{name:'下一步',exact:true}).click();await expect(lab.getByTestId('irq-flag')).toHaveText('1');await expect(lab.getByTestId('irq-lane')).toHaveText('中断处理');
  await lab.getByRole('button',{name:'下一步',exact:true}).click();await expect(lab.getByTestId('irq-lane')).toHaveText('主循环');
  for(let i=0;i<3;i++)await lab.getByRole('button',{name:'下一步',exact:true}).click();await expect(lab.getByTestId('irq-flag')).toHaveText('0');await expect(lab.getByTestId('irq-handled')).toHaveText('1');
  await noOverflow(page);await lab.screenshot({path:info.outputPath('interrupt.png')});await lab.getByRole('button',{name:'重置',exact:true}).click();await expect(lab.getByTestId('irq-handled')).toHaveText('0');
});
test('search and theme navigation remain usable',async({page},info)=>{
  await page.goto('./');await page.getByRole('button',{name:'搜索课程',exact:true}).click();
  const input=page.locator('input[type=search]');await expect(input).toBeVisible();await input.fill('占空比');
  await expect(page.locator('.VPLocalSearchBox')).toContainText('PWM');await page.keyboard.press('Escape');
  if(info.project.name!=='desktop')await page.locator('.VPNavBarHamburger').click();
  const toggle=page.locator('.VPSwitchAppearance:visible').first();await toggle.click();await expect(page.locator('html')).toHaveClass(/dark/);
  if(info.project.name!=='desktop')await page.locator('.VPNavBarHamburger').click();
  await noOverflow(page);await page.screenshot({path:info.outputPath('dark-home.png'),fullPage:true});
});
