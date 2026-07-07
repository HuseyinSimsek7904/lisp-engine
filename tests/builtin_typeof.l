(include "core/assert.l")

;; `typeof` structure: (typeof VALUE)

;; Wrong number of arguments..
(assert-eq (typeof) (error (list (quote wrong-number-of-args-to-builtin) typeof 0 1)))
(assert-eq (typeof 1 2) (error (list (quote wrong-number-of-args-to-builtin) typeof 2 1)))

;; Testing of type checking is performed in tests/types.l.
