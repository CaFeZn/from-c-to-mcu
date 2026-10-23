import { defineConfig, devices } from '@playwright/test';
export default defineConfig({
  testDir:'tests/browser', timeout:45000, expect:{timeout:10000}, fullyParallel:false,
  use:{baseURL:process.env.BASE_URL||'http://127.0.0.1:4318/from-c-to-mcu/',trace:'retain-on-failure',screenshot:'only-on-failure',launchOptions:{channel:process.env.CI?'chromium':'chrome'}},
  projects:[{name:'desktop',use:{viewport:{width:1440,height:1000}}},{name:'mobile',use:{...devices['iPhone 13'],defaultBrowserType:'chromium'}},{name:'narrow',use:{viewport:{width:320,height:800}}}],
  reporter:[['list'],['html',{open:'never'}]]
});
