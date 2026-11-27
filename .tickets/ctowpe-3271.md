---
id: ctowpe-3271
status: closed
deps: [ctowpe-eg19]
links: []
created: 2026-11-27T11:20:44Z
type: task
priority: 1
assignee: kmd
parent: ctopo-yhkb
tags: [offscreen, streaming, physics]
---
# Run native oil and slime pair passes across resident and offscreen water owners

## Design

Record the native bulk phase pair, retention and thermal exchanges on a shared domain after the scheduled water phases on every elapsed tick. Validate and publish stock and the carried momentum and heat atomically. Enable camera election of connected bulk liquid owners and keep ordinary owner phases ordered.

## Acceptance Criteria

Match monolithic horizontal and vertical native ticks with differing oil/slime/water stocks, momentum, heat, retention, aperture and flow direction. Include partial water cadence and an elapsed tick with zero scheduled water phases. A rejected or unconfirmed receipt must not publish one side.


## Notes

**2026-11-27T12:17:10Z**

The seven-case native cohort validation and camera bulk fixture cover retention, horizontal and vertical phase flux, heat exchange, both momenta, cadence and rejected publication; the moving obstruction dependency is closed.
