import { expect, test } from '@playwright/test';

test('uses the public landing page to enter the dashboard workbench', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByRole('heading', { name: /watch a program become machine code/i })).toBeVisible();
  await Promise.all([
    page.waitForURL(/\/lab$/),
    page.getByRole('link', { name: /open compiler lab/i }).click(),
  ]);
  await expect(page.getByRole('navigation', { name: 'Lab views' })).toBeVisible();
  await expect(page.getByRole('link', { name: 'Workbench' })).toBeVisible();
});

test('inspects compiler stages, graphs, execution profile, and adaptive events', async ({ page }) => {
  await page.goto('/lab');
  await expect(page.locator('.compile-status')).toContainText('verified');
  await expect(page.getByLabel('Compiler revision')).toHaveText('0.3.0+phase11-dev');

  const terminalCommand = page.getByRole('textbox', { name: 'Terminal command' });
  await terminalCommand.fill('verify');
  await terminalCommand.press('Enter');
  await expect(page.getByText(/PASS · 1 function/)).toBeVisible();

  await page.getByLabel('IR stage').selectOption('ssa');
  await expect(page.getByLabel('IR viewer')).toContainText('func @main');
  await page.getByRole('button', { name: /Nano SSA Optimization @main/ }).click();
  await expect(page.getByText('Before / after')).toBeVisible();

  await page.getByLabel('Graph kind').selectOption('dominator-tree');
  await expect(page.getByLabel('dominator-tree graph')).toBeVisible();
  await page.getByLabel('Graph kind').selectOption('cfg');
  await page.getByText('bb1.then', { exact: true }).click();

  await page.getByRole('button', { name: 'ssa', exact: true }).click();
  await expect(page.getByText(/^\d+ calls$/)).toBeVisible();

  await page.getByLabel('Example program').selectOption('Adaptive JIT');
  await expect(page.locator('.compile-status')).toContainText('verified');
  await page.getByRole('button', { name: 'adaptive-jit', exact: true }).click();
  await expect(page.getByText('Adaptive JIT events')).toBeVisible();
});
