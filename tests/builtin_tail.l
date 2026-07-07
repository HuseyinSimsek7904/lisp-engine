(include "core/assert.l")

;; `tail` structure: (tail LIST)

;; Wrong number of arguments.
(assert-eq (tail) (error (list (quote wrong-number-of-args-to-builtin) tail 0 1)))
(assert-eq (tail 1 2) (error (list (quote wrong-number-of-args-to-builtin) tail 2 1)))

;; Wrong type of argument.
(assert-eq (tail 1) (error (list (quote wrong-type-arg-to-builtin) tail 1 list-t)))
(assert-eq (tail "abc") (error (list (quote wrong-type-arg-to-builtin) tail "abc" list-t)))
(assert-eq (tail nil) (error (list (quote wrong-type-arg-to-builtin) tail nil list-t)))

;; Functionality test.
(assert-eq (tail (list 1 2 3)) (list 2 3))
(assert-eq (tail (list 1 2)) (list 2))
(assert-eq (tail (list 1)) nil)
(assert-eq (tail (list nil 1 2 3)) (list 1 2 3))
(assert-eq (tail (list nil)) nil)
