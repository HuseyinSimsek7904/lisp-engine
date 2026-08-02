(include "core/assert.l")

;; `cons` structure: (cons HEAD TAIL)

;; Wrong number of arguments.
(assert-eq (cons)
           (error `(wrong-number-of-args-to-builtin ~cons 0 2)))
(assert-eq (cons 1)
           (error `(wrong-number-of-args-to-builtin ~cons 1 2)))
(assert-eq (cons 1 2 3)
           (error `(wrong-number-of-args-to-builtin ~cons 3 2)))
(assert-eq (cons 1 2 3 4)
           (error `(wrong-number-of-args-to-builtin ~cons 4 2)))

;; Wrong type of arguments.
(assert-eq (cons "abc" "def")
           (error `(wrong-type-arg-to-builtin ~cons "def" ~list-t)))

;; Error as argument.
(assert-eq (cons "anything" (error "should not return"))
           (error `(wrong-type-arg-to-builtin ~cons ~(error "should not return") ~list-t)))

;; Behavior tests.
(assert-eq (cons () ()) '(()))
(assert-eq (cons 123 ()) '(123))
(assert-eq (cons "123" ()) '("123"))
(assert-eq (cons 1 '(2 3 4)) '(1 2 3 4))
