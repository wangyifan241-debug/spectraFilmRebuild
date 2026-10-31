# README history cleanup

Explicitly authorized scope: both root README files, across all branches in
`wangyifan241-debug/spectraFilmRebuild`. The original upstream repository was not
modified. Its attribution, code history, licenses and material notices remain.

- Root `README.md` and `README.zh-CN.md` were rewritten for the Windows port.
- Official git-filter-repo v2.47.0 was used in an isolated bare repository, not
  on the working tree. Tool SHA256:
  `67447413e273fc76809289111748870b6f6072f08b17efe94863a92d810b7d94`.
- A verified, complete pre-cleanup bundle remains **local only**. Backup refs
  were not pushed. Old commit IDs/maps are also kept locally, not published here.
- **540 historical trees** were independently compared excluding only the two
  README paths: no non-README file content changed. **7 newly empty README-only
  commits** were pruned. Mixed commits retain their non-README changes.
- The code-branch tip has an identical tree to the tested pre-cleanup tip after
  reintroducing the new README files. Physical build-input SHA256 is unchanged.
- Both remote branches were updated atomically using explicit per-ref
  force-with-lease expectations and TLS verification, then fetched and checked.
- Both root README paths now have only the new introduction commit in each
  branch's reachable history. Main is a new project landing page; the existing
  code-branch arrangement is retained, not silently merged onto main.

Code branch: `codex/windows-materials-ui-20261031`.

The change does not prove physical deletion from GitHub storage, cached commit
pages, PR references, forks or other people's clones. See
[GitHub's history-removal guidance](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/removing-sensitive-data-from-a-repository).
Collaborators should re-clone or carefully reconcile their clones; do not push
old branches/backups or use `push --all`/`push --mirror` to reintroduce removed
history. This task did not request deletion of unrelated code history.
