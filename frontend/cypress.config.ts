import { defineConfig } from "cypress";

export default defineConfig({
  e2e: {
    // The tests run against the Vite dev server: start it with `pnpm run serve` first
    baseUrl: "http://localhost:5173",
    viewportWidth: 1280,
    viewportHeight: 800,
  },
});
