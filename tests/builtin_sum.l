(include "core/assert.l")

;; `+` structure: (+ INT64...)

;; Wrong type of argument.
(assert-eq (+ "abc" 1 2)
           (error `(wrong-type-arg-to-builtin ~+ "abc" ~int64-t)))
(assert-eq (+ 1 "abc" 2)
           (error `(wrong-type-arg-to-builtin ~+ "abc" ~int64-t)))
(assert-eq (+ 1 2 "abc")
           (error `(wrong-type-arg-to-builtin ~+ "abc" ~int64-t)))

;; Error as argument.
(assert-eq (+ (error "should not return") "anything")
           (error `(wrong-type-arg-to-builtin ~+ ~(error "should not return") ~int64-t)))
(assert-eq (+ "anything" (error "should not return"))
           (error `(wrong-type-arg-to-builtin ~+ "anything" ~int64-t)))

;; Sum tests.
(assert-eq (+) 0)
(assert-eq (+ 12) 12)
(assert-eq (+ 12 23) 35)
(assert-eq (+ 12 23 34) 69)
