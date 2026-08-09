(include "core/assert.l")

;; `set` structure: (set SYM VALUE)

;; Wrong number of arguments.
(assert-eq (set)
           (error `(wrong-number-of-args-to-builtin ~set 0 2)))
(assert-eq (set 1)
           (error `(wrong-number-of-args-to-builtin ~set 1 2)))
(assert-eq (set 1 2 3)
           (error `(wrong-number-of-args-to-builtin ~set 3 2)))
(assert-eq (set 1 2 3 4)
           (error `(wrong-number-of-args-to-builtin ~set 4 2)))

;; Wrong type of arguments.
(assert-eq (set "abc" 1)
           (error `(wrong-type-arg-to-builtin ~set "abc" ~sym-t)))

;; Behavior tests.
(let ((a 1) (b 2))
  (assert-eq a 1)
  (assert-eq b 2))

(let ((a 3))
  (let ((b 4))
    (assert-eq a 3)
    (assert-eq b 4)))

(let ((x 1))
  (assert-eq (set x 2) (error '(redefinition x))))
