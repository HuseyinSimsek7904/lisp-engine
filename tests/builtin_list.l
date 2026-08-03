(include "core/assert.l")

;; `list` structure: (list [ITEM...])

(assert-eq (list) empty)
(assert-eq (list) '())

;; Check against `quote`.
(assert-eq (list 1) '(1))
(assert-eq (list 1 2) '(1 2))
(assert-eq (list 1 2 3) '(1 2 3))
