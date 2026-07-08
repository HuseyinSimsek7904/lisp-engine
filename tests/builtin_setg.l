(include "core/assert.l")

;; `setg` structure: (setg SYM VALUE)

;; Wrong number of arguments.
(assert-eq (setg)
           (error (list 'wrong-number-of-args-to-builtin setg 0 2)))
(assert-eq (setg 1)
           (error (list 'wrong-number-of-args-to-builtin setg 1 2)))
(assert-eq (setg 1 2 3)
           (error (list 'wrong-number-of-args-to-builtin setg 3 2)))
(assert-eq (setg 1 2 3 4)
           (error (list 'wrong-number-of-args-to-builtin setg 4 2)))

;; Wrong type of arguments.
(assert-eq (setg "abc" 1)
           (error (list 'wrong-type-arg-to-builtin setg "abc" sym-t)))

;; Behavior tests.
(assert-eq a (error (list 'undefined-symbol 'a)))
(assert-eq b (error (list 'undefined-symbol 'b)))

(setg a 34)
(setg b 35)

(assert-eq a 34)
(assert-eq b 35)

(let ((a nil) (b nil))
  (assert-eq a nil)
  (assert-eq b nil)
  (setg a 4)
  (setg b 5)
  (assert-eq a nil)
  (assert-eq b nil))

(assert-eq a 4)
(assert-eq b 5)

(let ((a nil))
  (let ((b nil))
    (assert-eq a nil)
    (assert-eq b nil)
    (setg a "hi")
    (setg b "hello")
    (assert-eq a nil)
    (assert-eq b nil)))

(assert-eq a "hi")
(assert-eq b "hello")
