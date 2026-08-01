const root = document.documentElement;
const repository = root.dataset.githubRepository.trim().replace(/\/$/, "");
const releaseVersion = root.dataset.releaseVersion?.trim();
const downloadLinks = document.querySelectorAll("[data-release-link]");

function setDownloadUrl(url, label) {
  downloadLinks.forEach((link) => {
    link.href = url;
    if (label) link.title = label;
  });
}

async function resolveLatestRelease() {
  const repositoryPath = new URL(repository).pathname.replace(/^\//, "");
  const response = await fetch(`https://api.github.com/repos/${repositoryPath}/releases?per_page=10`, {
    headers: { Accept: "application/vnd.github+json" },
  });

  if (!response.ok) throw new Error(`GitHub release request failed: ${response.status}`);

  const releases = await response.json();
  const release = releases.find((candidate) => !candidate.draft);
  if (!release) throw new Error("No published RetroShell release found");

  const asset = release.assets.find((candidate) =>
    /^RetroShell-PSP-.*\.zip$/i.test(candidate.name)
  );

  setDownloadUrl(
    asset?.browser_download_url || release.html_url,
    asset ? `Download ${release.name || release.tag_name}` : `View ${release.name || release.tag_name}`
  );
}

if (repository) {
  document.querySelectorAll("[data-github-link]").forEach((link) => {
    link.href = repository;
    link.target = "_blank";
    link.rel = "noopener noreferrer";
  });
  setDownloadUrl(
    releaseVersion
      ? `${repository}/releases/download/${releaseVersion}/RetroShell-PSP-${releaseVersion}.zip`
      : `${repository}/releases`,
    "Download the latest RetroShell release"
  );
  resolveLatestRelease().catch(() => {
    // The versioned URL above remains usable if GitHub's API is unavailable.
  });
  document.querySelectorAll("[data-github-doc]").forEach((link) => {
    link.href = `${repository}/blob/main/${link.dataset.githubDoc}`;
  });
} else {
  document.querySelectorAll("[data-github-link], [data-release-link]").forEach((link) => {
    link.removeAttribute("href");
    link.setAttribute("aria-disabled", "true");
    link.classList.add("is-disabled");
    link.title = "GitHub repository unavailable";
  });
}
