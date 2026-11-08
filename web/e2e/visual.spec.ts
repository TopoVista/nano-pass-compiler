import { expect, test } from '@playwright/test';

test.describe('visual baselines', () => {
  test('public landing remains editorial at desktop and mobile widths', async ({ page }) => {
    await page.setViewportSize({ width: 1440, height: 980 });
    await page.goto('/');
    await expect(page).toHaveScreenshot('landing-desktop.png', {
      animations: 'disabled',
      fullPage: false,
    });

    await page.setViewportSize({ width: 390, height: 844 });
    await page.goto('/');
    await expect(page).toHaveScreenshot('landing-mobile.png', {
      animations: 'disabled',
      fullPage: false,
    });
  });

  test('workbench preserves the code-first shell without a compiler service', async ({ page }) => {
    await page.route('**/api/v1/**', (route) => route.abort());
    await page.setViewportSize({ width: 1440, height: 980 });
    await page.goto('/lab');
    await page.locator('.monaco-editor').waitFor();
    await expect(page.getByRole('button', { name: 'Retry compiler service' })).toBeVisible();
    await expect(page).toHaveScreenshot('workbench-desktop.png', {
      animations: 'disabled',
      fullPage: false,
    });
  });
});
