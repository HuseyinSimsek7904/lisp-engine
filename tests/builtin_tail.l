(include "core/assert.l")

;; `tail` structure: (tail LIST)

;; Wrong number of arguments.
(assert-eq (tail)
           (error (list 'too-few-args-to-builtin tail 0 1)))
(assert-eq (tail 1 2 3)
           (error (list 'too-many-args-to-builtin tail 3 2)))
(assert-eq (tail 1 2 3 4)
           (error (list 'too-many-args-to-builtin tail 4 2)))

;; Wrong type of argument.
(assert-eq (tail 1)
           (error (list 'wrong-type-arg-to-builtin tail 1 list-t)))
(assert-eq (tail "abc")
           (error (list 'wrong-type-arg-to-builtin tail "abc" list-t)))
(assert-eq (tail nil)
           (error (list 'wrong-type-arg-to-builtin tail nil list-t)))
(assert-eq (tail (list 1) 1) (error (list 'wrong-type-arg-to-builtin tail (list 1) 1)))
(assert-eq (tail (list 1 2) 2) (error (list 'wrong-type-arg-to-builtin tail (list 1 2) 2)))
(assert-eq (tail (list 1 2 3) 3) (error (list 'wrong-type-arg-to-builtin tail (list 1 2 3) 3)))

;; Functionality test.
(assert-eq (tail (list 1)) nil)
(assert-eq (tail (list 1) 0) nil)

(assert-eq (tail (list 1 2)) (list 2))
(assert-eq (tail (list 1 2) 0) (list 2))
(assert-eq (tail (list 1 2) 1) nil)

(assert-eq (tail (list 1 2 3)) (list 2 3))
(assert-eq (tail (list 1 2 3) 0) (list 2 3))
(assert-eq (tail (list 1 2 3) 1) (list 3))
(assert-eq (tail (list 1 2 3) 2) nil)

(assert-eq (tail (list nil)) nil)
(assert-eq (tail (list nil) 0) nil)

(assert-eq (tail (list nil nil)) (list nil))
(assert-eq (tail (list nil nil) 0) (list nil))
(assert-eq (tail (list nil nil) 1) nil)

(assert-eq (tail (list nil nil nil)) (list nil nil))
(assert-eq (tail (list nil nil nil) 0) (list nil nil))
(assert-eq (tail (list nil nil nil) 1) (list nil))
(assert-eq (tail (list nil nil nil) 2) nil)
