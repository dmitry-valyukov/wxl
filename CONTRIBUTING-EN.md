# Contributing

*Русская версия: [CONTRIBUTING.md](CONTRIBUTING.md)*

The rules are the same for everyone who contributes. GitHub holds
them: the ruleset of `main` and the CI checks.

## The `main` branch

- `main` changes only through a pull request with green CI. A GitHub ruleset
  forbids direct pushes and force-pushes, requires linear history and allows
  merging by rebase or squash only. The merge queue merges a PR after it has
  checked what `main` would become; GitHub deletes a merged PR's branch.

## Branch and pull request

- A change lives on a branch named `wxl-<topic>`: lowercase letters and
  hyphens, such as `wxl-effects-catalog`.
- The pull request goes from that branch into `main`; one topic per PR.
- A branch need not be updated merely because `main` has moved on: the merge
  queue puts the PR on top of `main` itself and checks the result. When a
  branch does need updating, use the "Update with rebase" button. Do not
  merge `main` into the branch: linear history will not take the merge
  commit.

## CI

- [`.github/workflows/ci.yml`](.github/workflows/ci.yml) builds the tree in
  Debug and Release on a Windows runner, runs all tests and starts the
  `sample.hello` sample ([`samples/HelloHere`](samples/HelloHere)), which has
  to show its window. A third row, `Strict`, builds and runs the tests of
  `wxl.async` with the coroutine checks on (`WXL_STRICT_CORO`). The
  workflow's `commits` job checks the commits (below).
- The Windows build runs in the merge queue, on the candidate: `main`, the
  PRs queued ahead and this PR, that is, what would become `main`. If it
  fails, the PR leaves the queue and `main` stays as it is. The "Run
  workflow" button starts it by hand.
- A PR gets no Windows build of its own: its `build (…)` rows report
  success at once, so that the queue admits it. On the PR, the `commits` job
  and the documentation site do the checking.
- [`.github/workflows/docs.yml`](.github/workflows/docs.yml) builds the
  documentation site on Linux; a Doxygen warning is an error.
- The required checks are `build (Debug)`, `build (Release)`,
  `build (Strict)`, `build` (the documentation site) and `commits`; all of
  them have to be green.

## Commits

The `commits` job checks rules 1–5 on every commit of a pull request; its
error message names the rule.

1. Author and committer addresses are of the form
   `…@users.noreply.github.com` (GitHub's "Keep my email address private").
   A commit GitHub rewrites ("Update with rebase", rebase merge, the merge
   queue) gets whoever pressed the button as its committer, also such an
   address. The committer `GitHub <noreply@github.com>` is accepted only
   with a signature GitHub verifies.
2. The message has none of these lines: `Co-Authored-By` (a commit has one
   author), banner lines, trailer lines with links.
3. Hidden directories and files (`.…`) are not part of the repository; the
   tree keeps only those listed in the check.
4. A signature only if GitHub verifies it; an unsigned commit passes.
5. No merge commits: the history is linear.

Write the message in English in the industry style: a short imperative
subject, a blank line, and a body that says why.

## Code

- Before a pull request, check locally only what the change touches, in
  Debug: `tools\build.ps1 -Target <target>` and its own tests with
  `ctest --preset x64-debug -R <test>` (tests are built with
  `-Define BUILD_TESTING=ON`). The full matrix is CI's job, in the merge
  queue.
- Setup and build: [README-EN.md](README-EN.md#quick-start) and
  [docs/building.md](docs/building.md) (in Russian).
- Write comments and documentation like the neighbouring ones in the module;
  how the site is built and checked is in
  [docs/building.md](docs/building.md#сайт-документации).

## Bugs

The [`bugs/`](bugs) directory holds reproductions of defects reported to the
makers of third-party software.
