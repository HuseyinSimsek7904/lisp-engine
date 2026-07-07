(include "core/assert.l")

;; `head` structure: (head LIST)

;; Wrong number of arguments.
(assert-eq (head) (error (list (quote wrong-number-of-args-to-builtin) head 0 1)))
(assert-eq (head 1 2) (error (list (quote wrong-number-of-args-to-builtin) head 2 1)))

;; Wrong type of argument.
(assert-eq (head 1) (error (list (quote wrong-type-arg-to-builtin) head 1 list-t)))
(assert-eq (head "abc") (error (list (quote wrong-type-arg-to-builtin) head "abc" list-t)))
(assert-eq (head nil) (error (list (quote wrong-type-arg-to-builtin) head nil list-t)))

;; Functionality test.
(assert-eq (head (list 1 2 3)) 1)
(assert-eq (head (list 1 2)) 1)
(assert-eq (head (list 1)) 1)
(assert-eq (head (list nil 1 2 3)) nil)
(assert-eq (head (list nil)) nil)
