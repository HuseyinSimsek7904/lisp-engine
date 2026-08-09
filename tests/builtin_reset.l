(include "core/assert.l")

;; `reset` structure: (reset ATOM VALUE)

;; Wrong number of arguments.
(assert-eq (reset)
           (error `(wrong-number-of-args-to-builtin ~reset 0 2)))
(assert-eq (reset 1)
           (error `(wrong-number-of-args-to-builtin ~reset 1 2)))
(assert-eq (reset 1 2 3)
           (error `(wrong-number-of-args-to-builtin ~reset 3 2)))
(assert-eq (reset 1 2 3 4)
           (error `(wrong-number-of-args-to-builtin ~reset 4 2)))

;; Wrong type of arguments.
(assert-eq (reset "abc" 1)
           (error `(wrong-type-arg-to-builtin ~reset "abc" ~atom-t)))

;; Behavior tests.
(let ((a (atom nil)) (b (atom nil)))
  (assert-eq (deref a) nil)
  (assert-eq (deref b) nil)
  (reset a 4)
  (reset b 3)
  (assert-eq (deref a) 4)
  (assert-eq (deref b) 3))
