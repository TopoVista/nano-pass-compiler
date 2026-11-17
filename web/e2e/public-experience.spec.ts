import { expect, test } from '@playwright/test';

test.describe('public compiler story', () => {
  test('keeps the representative transformation controllable and clearly labeled', async ({ page }) => {
    await page.goto('/');

    await expect(page.getByText('static illustration · not a live compiler run')).toBeVisible();
    await page.getByRole('tab', { name: /verify ssa/i }).click();
    await expect(page.getByText('The representative SSA passes its structural check.')).toBeVisible();

    await page.getByRole('button', { name: /ssa optimizer/i }).click();
    await expect(page.getByText(/applies verifier-gated rewrites/i)).toBeVisible();
  });

  test('suppresses decorative animation when reduced motion is requested', async ({ page }) => {
    await page.emulateMedia({ reducedMotion: 'reduce' });
    await page.goto('/');

    await expect(page.locator('.hero-code-typing').first()).toHaveCSS('animation-name', 'none');
    await expect(page.locator('.hero-code-typing').first()).toHaveCSS('width', /\d+px|auto/);
  });
});
