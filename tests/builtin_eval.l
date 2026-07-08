(include "core/assert.l")

;; `eval` structure: (eval BODY...)

;; Wrong number of arguments.
(assert-eq (eval) (error (list 'too-few-args-to-builtin eval 0 1)))

;; `eval` is a function, unlike `do` which is a macro.
;; So every argument is evaluated first and then the function is called.
(assert-eq (eval (list 1 2 3)) (error (list 'not-callable 1)))
(assert-eq (eval 'abc) (error (list 'undefined-symbol 'abc)))

(let ((test nil))
  (assert-eq (eval (list + test 4) (set test 3) 'okay)
             (error (list 'wrong-type-arg-to-builtin + nil int64-t)))
  (assert-eq (eval '(+ test 4) (set test 3) ''okay) 'okay))

;; If anything inside `BODY` returns error, it is returned immediately.
(assert-eq
 (eval '(print nil)
       '(print "should not be printed"))
 (error (list 'wrong-type-arg-to-builtin print nil str-t)))

(assert-eq
 (eval '(set test 5)
       '(print nil))
 (error (list 'wrong-type-arg-to-builtin print nil str-t)))
