(include "core/assert.l")

;; `error` structure: (error VALUE)

;; Wrong number of arguments..
(assert-eq (error)
 (error (list 'wrong-number-of-args-to-builtin error 0 1)))
(assert-eq (error 1 2)
 (error (list 'wrong-number-of-args-to-builtin error 2 1)))
