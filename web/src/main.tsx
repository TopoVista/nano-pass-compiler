import { lazy, StrictMode, Suspense } from 'react';
import { createRoot } from 'react-dom/client';
import '@fontsource-variable/manrope/wght.css';
import '@fontsource/dm-serif-display/latin.css';
import '@fontsource/dm-serif-display/latin-italic.css';
import './styles.css';

const LandingPage = lazy(async () => ({
  default: (await import('./components/LandingPage')).LandingPage,
}));
const CompilerLab = lazy(() => import('./App'));

function RouteRoot() {
  const View = window.location.pathname === '/' ? LandingPage : CompilerLab;
  return (
    <Suspense fallback={<main className="route-loading" aria-live="polite">Loading NanoPass…</main>}>
      <View />
    </Suspense>
  );
}

createRoot(document.getElementById('root')!).render(<StrictMode><RouteRoot /></StrictMode>);
