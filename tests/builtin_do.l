(include "core/assert.l")

;; `do` structure: (do [BODY...])

;; If anything inside `BODY` returns error, it is returned immediately.
(let ((test (atom nil)))
  (assert-eq (deref test) nil)
  (assert-eq (do (print nil) (reset test 5))
             (error `(wrong-type-arg-to-builtin ~print ~nil ~str-t)))
  (assert-eq (deref test) nil)
  (assert-eq (do (reset test 5) (print nil) (reset test 7))
             (error `(wrong-type-arg-to-builtin ~print ~nil ~str-t)))
  (assert-eq (deref test) 5))

(assert-eq (do (print nil) (print "should not be printed"))
           (error `(wrong-type-arg-to-builtin ~print ~nil ~str-t)))
