# Contributing

*Русская версия: [CONTRIBUTING.md](CONTRIBUTING.md)*

This document describes how changes are made to the repository: branch
naming, pull requests, CI checks and commit requirements. These
requirements are enforced automatically by the GitHub ruleset of the `main`
branch and by CI jobs.

## The `main` branch

- Changes reach `main` only through a pull request whose required CI checks
  have all passed.
- The ruleset of `main` forbids direct pushes and force-pushes, requires
  linear history and allows merging by rebase or squash only.
- Pull requests are merged through the merge queue: before merging, CI
  checks the state that `main` will have. After the merge, GitHub deletes
  the pull request branch automatically.

## Branch and pull request

- A change is developed on a separate branch named `wxl-<topic>`, using
  lowercase letters and hyphens, for example `wxl-effects-catalog`.
- The pull request is opened from that branch into `main` and covers a
  single topic.
- There is no need to update a branch only because new commits have
  reached `main`: the merge queue applies the pull request on top of the
  current `main` and checks the result. If an update is required, use the "Update with rebase"
  button. Do not merge `main` into the branch: a merge commit violates the
  linear history requirement.

## CI

- The [`.github/workflows/ci.yml`](.github/workflows/ci.yml) workflow runs
  on a Windows runner. It builds the tree in the Debug and Release
  configurations, runs all tests and starts the `sample.hello` sample
  ([`samples/HelloHere`](samples/HelloHere)), which must open its window.
  The `Strict` configuration builds and runs the `wxl.async` tests with
  coroutine checks enabled (`WXL_STRICT_CORO`). The `commits` job of the
  same workflow checks the commits of the pull request (see "Commits").
- The Windows build runs in the merge queue and checks the merge candidate:
  the current `main`, the pull requests queued ahead and this pull request.
  If the build fails, the pull request is removed from the queue and `main`
  is left unchanged. The workflow can be started manually with the
  "Run workflow" button.
- A pull request itself gets no Windows build: the `build (…)` checks
  report success immediately so that the pull request can enter the merge
  queue. At the pull request stage, the `commits` job and the documentation
  site build are run.
- The [`.github/workflows/docs.yml`](.github/workflows/docs.yml) workflow
  builds the documentation site on Linux; any Doxygen warning is treated as
  an error.
- Required checks: `build (Debug)`, `build (Release)`, `build (Strict)`,
  `build` (the documentation site) and `commits`. A pull request is merged
  only when all of them have passed.

## Commits

The `commits` job checks every commit of a pull request against rules 1–5;
the error message gives the number of the violated rule.

1. Author and committer email addresses have the form
   `…@users.noreply.github.com` (GitHub's "Keep my email address private"
   setting). When GitHub recreates a commit ("Update with rebase", rebase
   merge, the merge queue), the user who pressed the button becomes the
   committer, and their address must have the same form. The committer
   `GitHub <noreply@github.com>` is accepted only on a commit with a
   signature verified by GitHub.
2. The commit message contains no `Co-Authored-By` lines (a commit has one
   author), no banner lines and no trailer lines with links.
3. Hidden directories and files (names starting with a dot) are not added
   to the repository, except those listed in the check.
4. Signing a commit is optional; if a commit is signed, GitHub must verify
   the signature.
5. Merge commits are not allowed: the history is linear.

Write the commit message in English in the conventional form: a short
subject in the imperative mood, a blank line, then a description of what
was changed and why.

## Code

- Before opening a pull request, it is enough to build and test in the
  Debug configuration only the targets affected by the change:
  `tools\build.ps1 -Target <target>` and the related tests with
  `ctest --preset x64-debug -R <test>` (tests are built with
  `-Define BUILD_TESTING=ON`). The full configuration matrix is checked by
  CI in the merge queue.
- Machine setup and building are described in
  [README-EN.md](README-EN.md#quick-start) and
  [docs/building.md](docs/building.md) (in Russian).
- Comments and documentation follow the style of the neighbouring ones in
  the same module. Building and checking the documentation site are
  described in [docs/building.md](docs/building.md#сайт-документации).

## Bugs

The [`bugs/`](bugs) directory contains reproductions of defects reported to
the developers of third-party software.
