const root = document.documentElement;
const repository = root.dataset.githubRepository.trim().replace(/\/$/, "");
const releaseVersion = root.dataset.releaseVersion?.trim();

if (repository) {
  document.querySelectorAll("[data-github-link]").forEach((link) => {
    link.href = repository;
  });
  document.querySelectorAll("[data-release-link]").forEach((link) => {
    link.href = releaseVersion
      ? `${repository}/releases/download/${releaseVersion}/RetroShell-PSP-${releaseVersion}.zip`
      : `${repository}/releases/latest`;
  });
  document.querySelectorAll("[data-github-doc]").forEach((link) => {
    link.href = `${repository}/blob/main/${link.dataset.githubDoc}`;
  });
} else {
  document.querySelectorAll("[data-github-link]").forEach((link) => {
    link.removeAttribute("href");
    link.setAttribute("aria-disabled", "true");
    link.classList.add("is-disabled");
    link.title = "GitHub repository link coming with the first public release";
  });
}
