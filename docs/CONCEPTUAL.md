# Conceptual branch notes

Very experimental work. This branch lives in its own folder, `D:\PhantomDust-Conceptual` (a git worktree of
`D:\PhantomDust-Dumped-NoMovie\_netmod`), so a master session and a Conceptual session can run at the same time.

## Things to know
- **One game install.** Whichever kit's Setup.bat was run last is the one the game runs, so testing a Conceptual kit
  and a master build means switching between them with their Setup.bat. To test a Conceptual kit: unzip it to its own
  folder and run its Setup.bat. To go back to the normal builds: run Setup.bat in the Experimental 72 folder again.
- **Merging master in happens here.** Git won't check out Conceptual in the main folder while this folder has it, so
  merges are done from this folder: `git merge master`, then `git push origin Conceptual`. Changes go from master into
  Conceptual only, never back.

## Rules
- Commit only to Conceptual; push only to `origin Conceptual`. Never force-push.
- Kits: `.\build.bat conceptual "what it tries"` saves `PhantomDust-pdnet Conceptual <N>.zip` on D:\. Never published,
  never auto-updated. Never `build.bat experimental` or `promote` here.
- Notes for this work go in this file, not in HANDOFF.md; tools\kit-counter.txt stays untouched.
- `deps\eos-sdk` and `private\` are links to the main repo's; never commit private\ files, eos.ini, links.ini or game
  files.

## Log
(Newest first: what was tried, what happened.)
