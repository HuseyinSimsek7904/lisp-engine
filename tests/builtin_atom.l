(include "core/assert.l")

;; `atom` structure: (atom VALUE)

;; Wrong number of arguments..
(assert-eq (atom)
 (error `(wrong-number-of-args-to-builtin ~atom 0 1)))
(assert-eq (atom 1 2)
 (error `(wrong-number-of-args-to-builtin ~atom 2 1)))
