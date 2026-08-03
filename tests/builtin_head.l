(include "core/assert.l")

;; `head` structure: (head LIST)

;; Wrong number of arguments.
(assert-eq (head)
           (error `(too-few-args-to-builtin ~head 0 1)))
(assert-eq (head 1 2 3)
           (error `(too-many-args-to-builtin ~head 3 2)))
(assert-eq (head 1 2 3 4)
           (error `(too-many-args-to-builtin ~head 4 2)))

;; Wrong type of argument.
(assert-eq (head 1)
           (error `(wrong-type-arg-to-builtin ~head 1 ~list-t)))
(assert-eq (head "abc")
           (error `(wrong-type-arg-to-builtin ~head "abc" ~list-t)))

(assert-eq (head '())
           (error `(wrong-type-arg-to-builtin ~head () 0)))
(assert-eq (head '(1) 1)
           (error `(wrong-type-arg-to-builtin ~head (1) 1)))
(assert-eq (head '(1 2) 2)
           (error `(wrong-type-arg-to-builtin ~head (1 2) 2)))
(assert-eq (head '(1 2 3) 3)
           (error `(wrong-type-arg-to-builtin ~head (1 2 3) 3)))

;; Functionality test.
(assert-eq (head '(1)) 1)
(assert-eq (head '(1) 0) 1)

(assert-eq (head '(1 2)) 1)
(assert-eq (head '(1 2) 0) 1)
(assert-eq (head '(1 2) 1) 2)

(assert-eq (head '(1 2 3)) 1)
(assert-eq (head '(1 2 3) 0) 1)
(assert-eq (head '(1 2 3) 1) 2)
(assert-eq (head '(1 2 3) 2) 3)

(assert-eq (head '(())) '())
(assert-eq (head '(()) 0) '())

(assert-eq (head '(() ())) '())
(assert-eq (head '(() ()) 0) '())
(assert-eq (head '(() ()) 1) '())

(assert-eq (head '(() () ())) '())
(assert-eq (head '(() () ()) 0) '())
(assert-eq (head '(() () ()) 1) '())
(assert-eq (head '(() () ()) 2) '())
