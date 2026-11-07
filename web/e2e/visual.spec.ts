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

  test('workbench preserves the code-first shell', async ({ page }) => {
    await page.setViewportSize({ width: 1440, height: 980 });
    await page.goto('/lab');
    await expect(page.locator('.compile-status')).toContainText('verified');
    await expect(page).toHaveScreenshot('workbench-desktop.png', {
      animations: 'disabled',
      fullPage: false,
      mask: [page.locator('.compile-status'), page.locator('.compiler-revision')],
    });
  });
});
