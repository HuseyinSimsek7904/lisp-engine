(include "core/assert.l")

;; `cons` structure: (cons HEAD TAIL)

;; Wrong number of arguments.
(assert-eq (cons)
           (error (list 'wrong-number-of-args-to-builtin cons 0 2)))
(assert-eq (cons 1)
           (error (list 'wrong-number-of-args-to-builtin cons 1 2)))
(assert-eq (cons 1 2 3)
           (error (list 'wrong-number-of-args-to-builtin cons 3 2)))
(assert-eq (cons 1 2 3 4)
           (error (list 'wrong-number-of-args-to-builtin cons 4 2)))

;; Wrong type of arguments.
(assert-eq (cons "abc" "def")
           (error (list 'wrong-type-arg-to-builtin cons "def" list-t)))

;; Error as argument.
(assert-eq (cons "anything" (error "should not return"))
           (error (list 'wrong-type-arg-to-builtin cons (error "should not return") list-t)))

;; Behavior tests.
(assert-eq (cons nil nil) (list nil))
(assert-eq (cons 123 nil) (list 123))
(assert-eq (cons "123" nil) (list "123"))
(assert-eq (cons 1 (list 2 3 4)) (list 1 2 3 4))
