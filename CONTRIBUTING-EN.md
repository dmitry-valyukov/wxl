# Contributing

*Русская версия: [CONTRIBUTING.md](CONTRIBUTING.md)*

This document describes how changes are made to the repository: branch
naming, pull requests, CI checks and commit requirements. These
requirements are enforced automatically by the GitHub ruleset of the `main`
branch and by CI jobs. Only one requirement is not checked automatically: a
branch that has already been pushed is not rewritten (see "Branch and pull
request").

## The `main` branch

- Changes reach `main` only through a pull request whose required CI checks
  have all passed.
- The ruleset of `main` forbids direct pushes and force-pushes, requires
  linear history and allows merging by squash only.
- Pull requests are merged through the merge queue: before merging, CI
  checks the state that `main` will have, and the queue then squashes the
  pull request into one commit. Each pull request therefore results in one
  commit on `main`. After the merge, GitHub deletes the pull request branch
  automatically.

## Branch and pull request

- A change is developed on a separate branch named `wxl-<topic>`, using
  lowercase letters and hyphens, for example `wxl-effects-catalog`.
- The pull request is opened from that branch into `main` and covers a
  single topic.
- The pull request title becomes the subject of its commit on `main`, and
  the description becomes the commit body. Write them by the same rules as
  a commit message (see "Commits"); rule 2 applies to them as well.
- There is no need to update a branch only because new commits have
  reached `main`: the merge queue applies the pull request on top of the
  current `main` and checks the result.
- A branch that has already been pushed is not rewritten: force-pushes and
  rebases of pushed commits are not allowed. If a branch is behind `main`
  or conflicts with it, merge `main` into the branch with the "Update with
  merge commit" option of the "Update branch" button (not "Update with
  rebase"). Resolve a conflict in GitHub's "Resolve conflicts" editor or
  locally: merge the current `main` of this repository into the branch,
  resolve the conflict in the merge commit and push as usual.

## CI

- The [`.github/workflows/ci.yml`](.github/workflows/ci.yml) workflow runs
  on a Windows runner. It builds the tree in the Debug and Release
  configurations, runs all tests and starts the `sample.hello` sample
  ([`samples/HelloHere`](samples/HelloHere)), which must open its window.
  The `Strict` configuration builds and runs the async tests
  (`wxl.core/tests/async`) with coroutine checks enabled (`WXL_STRICT_CORO`). The `commits` job of the
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
- CI restores the NuGet packages in locked mode (`RestoreLockedMode=true`):
  a change of the pins (`tools/packages.json`) comes in one commit with the
  `tools/packages.lock.json` that the package update
  (`tools\build.ps1 -UpdatePackages`) wrote; newer versions under the same
  pins come the same way, with that file alone. A lock file that does not
  match the pins and a missing one stop the configure
  (`cmake/wxl_packages.cmake`; see
  [docs/building.md](docs/building.md#пакеты-nuget-и-проекция-cppwinrt)).
- Required checks: `build (Debug)`, `build (Release)`, `build (Strict)`,
  `build` (the documentation site) and `commits`. A pull request is merged
  only when all of them have passed.

## Commits

The `commits` job checks every commit of a pull request against rules 1–5,
and in the merge queue it checks the commit the queue squashes the pull
request into; the error message gives the number of the violated rule.

1. Author and committer email addresses have the form
   `…@users.noreply.github.com` (GitHub's "Keep my email address private"
   setting). The committer `GitHub <noreply@github.com>` appears on commits
   that GitHub creates itself (merges made with "Update with merge commit"
   and "Resolve conflicts", the squash in the merge queue); such a commit is
   accepted only with a signature verified by GitHub.
2. The commit message contains no `Co-Authored-By` lines (a commit has one
   author), no banner lines and no trailer lines with links. The same
   applies to the pull request title and description, which form the
   message of the squashed commit.
3. Hidden directories and files (names starting with a dot) are not added
   to the repository, except those listed in the check.
4. Signing a commit is optional; if a commit is signed, GitHub must verify
   the signature.
5. A merge commit is allowed only if it merges `main` into the branch: it
   has two parents, and the second one is a commit of `main`. Any other
   merge commit fails the check. The history of `main` itself stays linear,
   because the queue squashes each pull request into one commit.

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
