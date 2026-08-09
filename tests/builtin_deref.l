(include "core/assert.l")

;; `deref` structure: (deref ATOM)

;; Wrong number of arguments..
(assert-eq (deref)
 (error `(wrong-number-of-args-to-builtin ~deref 0 1)))
(assert-eq (deref 1 2)
 (error `(wrong-number-of-args-to-builtin ~deref 2 1)))
