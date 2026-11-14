---
name: github-cli
description: 'Use the GitHub CLI (gh) for every GitHub operation in ToshiNamura/XenonCLI: issues, pull requests, Actions, releases, repository metadata, search and raw REST calls. Use when the user asks to create, read, update, review, merge, close, search or automate anything on GitHub.'
---

# GitHub CLI — ToshiNamura/XenonCLI

The single workflow for interacting with GitHub from this repository. Prefer the
**GitHub CLI** (`gh`) over raw HTTP; use the REST API only when `gh` has no
dedicated command or when a raw payload is required.

## Repository constants

- Owner: `ToshiNamura`
- Repository: `XenonCLI`
- Default branch: `main`
- API base URL: `https://api.github.com`

Run commands from the repository root when possible; otherwise pass
`--repo ToshiNamura/XenonCLI` explicitly.

## When to use

- Issues: create, read, list, comment, label, assign, close, reopen, search.
- Pull requests: create, read, list, review, comment, check out, merge, close.
- Actions: list workflows and runs, inspect logs, re-run or cancel a run.
- Releases: create, list, upload and download assets.
- Repository: inspect metadata, list branches, manage labels and settings.
- Search: issues, pull requests, commits, code and repositories.
- Anything else: `gh api` for a typed or raw REST call.

## Environment and authentication

```powershell
gh --version
gh auth status
gh auth login                 # interactive; prefer a token via environment in CI
$env:GH_TOKEN                 # never print the value
$env:GITHUB_TOKEN
```

A token needs the `repo` scope (classic PAT) or `Contents`, `Issues`,
`Pull requests` and `Actions` read/write (fine-grained PAT). Never echo, log or
commit a token.

If `gh` is not on `PATH` in an already-open terminal, use the full path:

```powershell
& "C:\Program Files\GitHub CLI\gh.exe" --version
```

On Windows PowerShell use `curl.exe`, not the `curl` alias, when falling back to
the REST API.

## Issues

```powershell
gh issue list  --repo ToshiNamura/XenonCLI --state open --limit 50
gh issue view  12 --repo ToshiNamura/XenonCLI --comments
gh issue create --repo ToshiNamura/XenonCLI --title "Title" --body-file body.md --label bug
gh issue comment 12 --repo ToshiNamura/XenonCLI --body "Comment"
gh issue edit 12 --repo ToshiNamura/XenonCLI --add-label enhancement --add-assignee @me
gh issue close 12 --repo ToshiNamura/XenonCLI --reason completed
gh issue reopen 12 --repo ToshiNamura/XenonCLI
gh issue develop 12 --repo ToshiNamura/XenonCLI --checkout   # linked branch
```

Machine-readable output:

```powershell
gh issue list --repo ToshiNamura/XenonCLI --json number,title,labels,state `
  --jq '.[] | "\(.number)\t\(.title)"'
```

## Pull requests

```powershell
gh pr list   --repo ToshiNamura/XenonCLI --state open
gh pr view   34 --repo ToshiNamura/XenonCLI --comments --files
gh pr diff   34 --repo ToshiNamura/XenonCLI
gh pr create --repo ToshiNamura/XenonCLI --base main --title "Title" --body-file pr.md --draft
gh pr checkout 34
gh pr review 34 --repo ToshiNamura/XenonCLI --comment --body "Looks good"
gh pr review 34 --repo ToshiNamura/XenonCLI --approve
gh pr review 34 --repo ToshiNamura/XenonCLI --request-changes --body "Please fix X"
gh pr comment 34 --repo ToshiNamura/XenonCLI --body "Comment"
gh pr checks 34 --repo ToshiNamura/XenonCLI --watch
gh pr merge 34 --repo ToshiNamura/XenonCLI --squash --delete-branch
gh pr close 34 --repo ToshiNamura/XenonCLI
```

Review a PR locally:

```powershell
gh pr checkout 34
git --no-pager diff main...HEAD
```

## GitHub Actions

```powershell
gh workflow list --repo ToshiNamura/XenonCLI
gh run list     --repo ToshiNamura/XenonCLI --limit 20
gh run view     123456 --repo ToshiNamura/XenonCLI --log-failed
gh run watch    123456 --repo ToshiNamura/XenonCLI --exit-status
gh run rerun    123456 --repo ToshiNamura/XenonCLI --failed
gh run cancel   123456 --repo ToshiNamura/XenonCLI
gh workflow run ci.yml --repo ToshiNamura/XenonCLI --ref main
```

## Releases

```powershell
gh release list  --repo ToshiNamura/XenonCLI
gh release view  v0.1.0 --repo ToshiNamura/XenonCLI
gh release create v0.1.0 --repo ToshiNamura/XenonCLI --generate-notes
gh release upload v0.1.0 out/build/x64-release/xenon.exe --repo ToshiNamura/XenonCLI
gh release download v0.1.0 --repo ToshiNamura/XenonCLI --dir out/release
```

## Repository, labels and search

```powershell
gh repo view ToshiNamura/XenonCLI
gh repo edit ToshiNamura/XenonCLI --description "Xenon CLI and VM"
gh label list --repo ToshiNamura/XenonCLI
gh label create regression --repo ToshiNamura/XenonCLI --color D73A4A
gh search issues  --repo ToshiNamura/XenonCLI "vm"
gh search prs     --repo ToshiNamura/XenonCLI "packer"
gh search commits --repo ToshiNamura/XenonCLI "assembler"
```

## Raw REST calls

`gh api` handles authentication, pagination and JSON formatting:

```powershell
gh api repos/ToshiNamura/XenonCLI
gh api 'repos/ToshiNamura/XenonCLI/issues?state=open' --jq '.[].title'
gh api repos/ToshiNamura/XenonCLI/issues/12/comments -f body="Comment"
gh api repos/ToshiNamura/XenonCLI/pulls/34/merge -X PUT -f merge_method=squash
gh api --method POST repos/ToshiNamura/XenonCLI/issues `
  -f title="Title" -f body="Body" -f "labels[]=bug"
gh api graphql -f query='{ repository(owner:"ToshiNamura", name:"XenonCLI") { issues(first:5) { nodes { number title } } } }'
```

REST fallback with `curl.exe` only when `gh` is unavailable:

```powershell
$h = @('-H', 'Accept: application/vnd.github+json', '-H', "Authorization: Bearer $env:GH_TOKEN")
curl.exe @h 'https://api.github.com/repos/ToshiNamura/XenonCLI/issues?state=open'
```

## Rules

- Confirm the target repository, issue/PR number and destructive action
  (close, merge, delete, force-push, release deletion) before running it.
- Use `--repo ToshiNamura/XenonCLI` unless the working directory is already the
  repository root.
- Always add at least one label when creating an issue; the repository uses the
  standard GitHub labels (`bug`, `enhancement`, `documentation`, `question`,
  `help wanted`, `good first issue`). Check `gh label list` first.
- Use Markdown in bodies and comments; put long bodies in a file and pass
  `--body-file` to avoid shell quoting problems.
- Match the language of the issue/PR when replying.
- Prefer `--json`/`--jq` over scraping human-readable output inside automation.
- Never print token values; never commit tokens or `.env` files.
- When a command needs write access and authentication is missing, stop and
  tell the user which `gh auth login` or token step is required.
