# RetroShell landing page

The site is intentionally dependency-free and can be hosted by GitHub Pages or any static file host.

Preview it locally from the repository root:

```sh
python3 -m http.server 4173 --directory website
```

Then open `http://localhost:4173`.

GitHub and release buttons are configured from `data-github-repository` on the root element in `index.html`.

Core support copy should remain synchronized with each core's `manifest.json`. “All models” means the core is marked `psp1000Safe`. “PSP-1000 pending” means the core remains hidden in PSP-1000 Safe Mode while 32 MB hardware qualification is incomplete.
