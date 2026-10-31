---
id: ct-w5rq
status: open
deps: []
links: [ct-li7d]
created: 2026-10-31T22:37:35Z
type: bug
priority: 1
assignee: kmd
parent: ctfs-81vw
tags: [fire, thermal, gpu]
---
# Resolve standalone body hot-skin energy budget failures

While reproducing ct-li7d on X11/RTX A2000, combustion_body_feedback_display_tests had two existing failures before the char implementation: rotated mixed temperature and hot skin both reported negative or large total energy drift in hot_skin_budget_case. The test target is outside make test, so investigate the sample/flux ledger and fixture accounting separately.

## Acceptance Criteria

Run the full standalone combustion_body_feedback_display_tests on this GPU; both hot-skin cases conserve solid/gas energy within their stated tolerance and preserve nonnegative parcel energy; no regression to bonfire ignition or make test.

