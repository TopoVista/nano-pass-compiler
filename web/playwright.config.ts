import { defineConfig } from '@playwright/test';

export default defineConfig({
  testDir: './e2e',
  timeout: 30_000,
  use: { baseURL: 'http://127.0.0.1:15173', browserName: 'chromium', headless: true },
  webServer: [
    {
      command: 'build/compiler/nano-server --port=18080',
      cwd: '..',
      url: 'http://127.0.0.1:18080/api/v1/health',
      reuseExistingServer: !process.env.CI,
      timeout: 30_000,
    },
    {
      command: 'npm run dev -- --host 127.0.0.1 --port=15173',
      url: 'http://127.0.0.1:15173',
      reuseExistingServer: !process.env.CI,
      timeout: 30_000,
      env: { VITE_NANO_SERVER_URL: 'http://127.0.0.1:18080' },
    },
  ],
});
