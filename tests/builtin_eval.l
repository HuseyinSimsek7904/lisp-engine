(include "core/assert.l")

;; `eval` structure: (eval BODY...)

;; Wrong number of arguments.
(assert-eq (eval) (error `(too-few-args-to-builtin ~eval 0 1)))

;; `eval` is a function, unlike `do` which is a macro.
;; So every argument is evaluated first and then the function is called.
(assert-eq (eval '(1 2 3)) (error `(not-callable 1)))
(assert-eq (eval 'abc) (error `(undefined-symbol abc)))

(let ((test (atom nil)))
  (assert-eq (eval `(+ ~(deref test) 4) `(reset test 1) 'okay)
             (error `(wrong-type-arg-to-builtin ~+ ~nil ~int64-t)))
  (assert-eq (deref test) nil)
  (reset test 2)
  (assert-eq (eval `(+ ~(deref test) 4) `(reset test 3) ''okay) 'okay)
  (assert-eq (deref test) 3))

;; If anything inside `BODY` returns error, it is returned immediately.
(assert-eq
 (eval '(print nil)
       '(print "should not be printed"))
 (error `(wrong-type-arg-to-builtin ~print ~nil ~str-t)))
