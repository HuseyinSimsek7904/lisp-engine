(include "core/assert.l")

;; `tail` structure: (tail LIST)

;; Wrong number of arguments.
(assert-eq (tail)
           (error `(too-few-args-to-builtin ~tail 0 1)))
(assert-eq (tail 1 2 3)
           (error `(too-many-args-to-builtin ~tail 3 2)))
(assert-eq (tail 1 2 3 4)
           (error `(too-many-args-to-builtin ~tail 4 2)))

;; Wrong type of argument.
(assert-eq (tail 1)
           (error `(wrong-type-arg-to-builtin ~tail 1 ~list-t)))
(assert-eq (tail "abc")
           (error `(wrong-type-arg-to-builtin ~tail "abc" ~list-t)))
(assert-eq (tail ())
           (error `(wrong-type-arg-to-builtin ~tail () ~list-t)))
(assert-eq (tail '(1) 1) (error `(wrong-type-arg-to-builtin ~tail (1) 1)))
(assert-eq (tail '(1 2) 2) (error `(wrong-type-arg-to-builtin ~tail (1 2) 2)))
(assert-eq (tail '(1 2 3) 3) (error `(wrong-type-arg-to-builtin ~tail (1 2 3) 3)))

;; Functionality test.
(assert-eq (tail '(1)) ())
(assert-eq (tail '(1) 0) ())

(assert-eq (tail '(1 2)) '(2))
(assert-eq (tail '(1 2) 0) '(2))
(assert-eq (tail '(1 2) 1) ())

(assert-eq (tail '(1 2 3)) '(2 3))
(assert-eq (tail '(1 2 3) 0) '(2 3))
(assert-eq (tail '(1 2 3) 1) '(3))
(assert-eq (tail '(1 2 3) 2) ())

(assert-eq (tail '(())) ())
(assert-eq (tail '(()) 0) ())

(assert-eq (tail '(() ())) '(()))
(assert-eq (tail '(() ()) 0) '(()))
(assert-eq (tail '(() ()) 1) ())

(assert-eq (tail '(() () ())) '(() ()))
(assert-eq (tail '(() () ()) 0) '(() ()))
(assert-eq (tail '(() () ()) 1) '(()))
(assert-eq (tail '(() () ()) 2) ())
