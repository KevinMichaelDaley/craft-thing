---
id: ctowt-o11u
status: open
deps: [ctowt-iyyp]
links: []
created: 2026-11-27T06:30:47Z
type: task
priority: 1
assignee: kmd
parent: ctopp-2c14
tags: [offscreen, streaming, physics]
---
# Retain water heat and pigment in the paired transport result

## Design

Include each owner cell's expected and final water color and thermal excess. Carry both using the existing transport shaders on isolated scratch, including multi-cell moves and a mix at a receiving cell.

## Acceptance Criteria

Check heat and pigment against native monolithic transport and preserve the live source fields. Exercise thermal-enabled and unheated owners.

