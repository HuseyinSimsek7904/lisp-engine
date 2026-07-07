(include "core/assert.l")

;; `<` structure: (< X Y)

;; Wrong number of arguments.
(assert-eq (<) (error (list (quote wrong-number-of-args-to-builtin) < 0 2)))
(assert-eq (< 1) (error (list (quote wrong-number-of-args-to-builtin) < 1 2)))
(assert-eq (< 1 2 3) (error (list (quote wrong-number-of-args-to-builtin) < 3 2)))
(assert-eq (< 1 2 3 4) (error (list (quote wrong-number-of-args-to-builtin) < 4 2)))

;; Wrong type of arguments.
(assert-eq (< "abc" 1) (error (list (quote wrong-type-arg-to-builtin) < "abc" int64-t)))
(assert-eq (< 1 "abc") (error (list (quote wrong-type-arg-to-builtin) < "abc" int64-t)))

;; Error as argument.
(assert-eq (< (error "this") "that") (error (list (quote wrong-type-arg-to-builtin) < (error "this") int64-t)))
(assert-eq (< "that" (error "this")) (error (list (quote wrong-type-arg-to-builtin) < "that" int64-t)))

;; Behavior tests.
(assert-eq (< 12 23) true)
(assert-eq (< 23 12) false)
(assert-eq (< -42 69) true)
(assert-eq (< 69 -42) false)
(assert-eq (< 0 25) true)
(assert-eq (< 25 0) false)
(assert-eq (< -25 0) true)
(assert-eq (< 0 -25) false)
(assert-eq (< 18446744073709551615 1) true)
(assert-eq (< 1 18446744073709551615) false)
(assert-eq (< 9223372036854775808 9223372036854775807) true)
(assert-eq (< 9223372036854775807 9223372036854775808) false)
