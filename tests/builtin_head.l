(include "core/assert.l")

;; `head` structure: (head LIST)

;; Wrong number of arguments.
(assert-eq (head)
           (error (list 'too-few-args-to-builtin head 0 1)))
(assert-eq (head 1 2 3)
           (error (list 'too-many-args-to-builtin head 3 2)))
(assert-eq (head 1 2 3 4)
           (error (list 'too-many-args-to-builtin head 4 2)))

;; Wrong type of argument.
(assert-eq (head 1)
           (error (list 'wrong-type-arg-to-builtin head 1 list-t)))
(assert-eq (head "abc")
           (error (list 'wrong-type-arg-to-builtin head "abc" list-t)))
(assert-eq (head nil)
           (error (list 'wrong-type-arg-to-builtin head nil list-t)))
(assert-eq (head (list 1) 1) (error (list 'wrong-type-arg-to-builtin head (list 1) 1)))
(assert-eq (head (list 1 2) 2) (error (list 'wrong-type-arg-to-builtin head (list 1 2) 2)))
(assert-eq (head (list 1 2 3) 3) (error (list 'wrong-type-arg-to-builtin head (list 1 2 3) 3)))

;; Functionality test.
(assert-eq (head (list 1)) 1)
(assert-eq (head (list 1) 0) 1)

(assert-eq (head (list 1 2)) 1)
(assert-eq (head (list 1 2) 0) 1)
(assert-eq (head (list 1 2) 1) 2)

(assert-eq (head (list 1 2 3)) 1)
(assert-eq (head (list 1 2 3) 0) 1)
(assert-eq (head (list 1 2 3) 1) 2)
(assert-eq (head (list 1 2 3) 2) 3)

(assert-eq (head (list nil)) nil)
(assert-eq (head (list nil) 0) nil)

(assert-eq (head (list nil nil)) nil)
(assert-eq (head (list nil nil) 0) nil)
(assert-eq (head (list nil nil) 1) nil)

(assert-eq (head (list nil nil nil)) nil)
(assert-eq (head (list nil nil nil) 0) nil)
(assert-eq (head (list nil nil nil) 1) nil)
(assert-eq (head (list nil nil nil) 2) nil)
