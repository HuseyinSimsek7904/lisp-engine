(include "core/assert.l")

;; `eval` structure: (eval BODY...)

;; Wrong number of arguments.
(assert-eq (eval) (error (list (quote too-few-args-to-builtin) eval 0 1)))

;; `eval` is a function, unlike `do` which is a macro.
;; So every argument is evaluated first and then the function is called.
(assert-eq (eval (list 1 2 3)) (error (list (quote not-callable) 1)))
(assert-eq (eval (quote abc)) (error (list (quote undefined-symbol) (quote abc))))

(let ((test nil))
  (assert-eq (eval (list + test 4) (set test 3) (quote okay)) (error (list (quote wrong-type-arg-to-builtin) + nil int64-t)))
  (assert-eq (eval (quote (+ test 4)) (set test 3) (quote (quote okay))) (quote okay)))

;; If anything inside `BODY` returns error, it is returned immediately.
(assert-eq (eval (quote (print nil)) (quote (print "should not be printed"))) (error (list (quote wrong-type-arg-to-builtin) print nil str-t)))
(assert-eq (eval (quote (set test 5)) (quote (print nil))) (error (list (quote wrong-type-arg-to-builtin) print nil str-t)))
