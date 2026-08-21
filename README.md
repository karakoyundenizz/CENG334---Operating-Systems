# CENG334 — Introduction to Operating Systems

METU Computer Engineering, 2025–2026 Spring. Three assignments, all in C/C++ on
Linux, all working directly against the kernel interface instead of a runtime.

| | Assignment | What it is | Core topic |
|---|---|---|---|
| [Hw1](Hw1) | Concurrent data pipeline orchestrator | A process controller that runs multi-stage CSV pipelines and merges their streams through an N-ary tree | `fork`/`exec`, pipes, Unix domain sockets, deadlock-free IPC |
| [Hw2](Hw2) | Crossroad traffic controller | A monitor that lets cars cross a shared intersection without collisions, deadlock or starvation | pthreads, monitors, condition variables, priority scheduling |
| [Hw3](Hw3) | `mergeext2fs` | A three-way merge of ext2 filesystem images — like `git merge`, but at the inode and block level | filesystem internals, ext2 on-disk layout, raw block I/O |

Each folder has its own README with the design notes, plus the original
assignment PDF.

## Building

Every assignment is a plain `make` away:

```bash
cd Hw2 && make        # -> ./hw2
cd Hw3 && make        # -> ./mergeext2fs
```

Written and graded on Linux (METU `inek` machines); Hw2 and Hw3 also build and
run on macOS with a recent clang.
