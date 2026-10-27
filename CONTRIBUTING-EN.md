# Contributing

*Русская версия: [CONTRIBUTING.md](CONTRIBUTING.md)*

The rules are the same for everyone who contributes. GitHub holds
them: the ruleset of `main` and the CI checks. One alone rests on the
contributor, since nothing checks it: a pushed branch is not rewritten
(below).

## The `main` branch

- `main` changes only through a pull request with green CI. A GitHub ruleset
  forbids direct pushes and force-pushes, requires linear history and allows
  merging by squash only. The merge queue merges a PR after it has checked
  what `main` would become, and squashes it into one commit: one PR, one
  commit on `main`. GitHub deletes a merged PR's branch.

## Branch and pull request

- A change lives on a branch named `wxl-<topic>`: lowercase letters and
  hyphens, such as `wxl-effects-catalog`.
- The pull request goes from that branch into `main`; one topic per PR.
- The PR title becomes the subject of its commit on `main` and the
  description its body: write them the way a commit message is written
  (below); rule 2 holds for them too.
- A branch need not be updated merely because `main` has moved on: the merge
  queue puts the PR on top of `main` itself and checks the result.
- A pushed branch is not rewritten: no force-push, no rebase of what is
  already pushed. When a branch is behind `main` or conflicts with it,
  `main` is merged into the branch: with "Update with merge commit" of the
  "Update branch" button (not "Update with rebase"); on a conflict, in
  GitHub's "Resolve conflicts" editor or locally, by merging the current
  `main` of this repository with the conflict resolved in the merge commit
  and pushing as usual.

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

The `commits` job checks rules 1–5 on every commit of a pull request and,
in the merge queue, on the commit the queue squashes it into; its error
message names the rule.

1. Author and committer addresses are of the form
   `…@users.noreply.github.com` (GitHub's "Keep my email address private").
   The committer `GitHub <noreply@github.com>`, that of the commits GitHub
   makes itself (the merges of "Update with merge commit" and "Resolve
   conflicts", the merge queue's squash), is accepted only with a signature
   GitHub verifies.
2. The message has none of these lines: `Co-Authored-By` (a commit has one
   author), banner lines, trailer lines with links. The same holds for the
   PR title and description, which become the message of the squashed
   commit.
3. Hidden directories and files (`.…`) are not part of the repository; the
   tree keeps only those listed in the check.
4. A signature only if GitHub verifies it; an unsigned commit passes.
5. A merge commit only merges `main` into the branch: two parents, the
   second of them a commit of `main`; any other merge fails the check.
   `main` itself stays linear: the queue squashes each PR into one commit.

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
