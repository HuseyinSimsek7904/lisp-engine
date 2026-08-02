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
(let ((a nil) (b nil))
  (assert-eq a nil)
  (assert-eq b nil)
  (set a 4)
  (assert-eq a 4)
  (assert-eq b nil))

(let ((a nil) (b nil))
  (assert-eq a nil)
  (assert-eq b nil)
  (set b 4)
  (assert-eq a nil)
  (assert-eq b 4))

(let ((a nil))
  (let ((b nil))
    (assert-eq a nil)
    (assert-eq b nil)
    (set b 4)
    (assert-eq a nil)
    (assert-eq b 4)))

(let ((a nil))
  (let ((b nil))
    (assert-eq a nil)
    (assert-eq b nil)
    (set a 4)
    (assert-eq a 4)
    (assert-eq b nil)))
