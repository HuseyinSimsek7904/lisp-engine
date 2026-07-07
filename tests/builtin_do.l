(include "core/assert.l")

;; `do` structure: (do BODY...)

;; Wrong number of arguments.
(assert-eq (do) (error (list (quote too-few-args-to-builtin) do 0 1)))

;; If anything inside `BODY` returns error, it is returned immediately.
(let ((test nil))
  (assert-eq test nil)
  (assert-eq (do (print nil) (set test 5)) (error (list (quote wrong-type-arg-to-builtin) print nil str-t)))
  (assert-eq test nil)
  (assert-eq (do (set test 5) (print nil) (set test 7)) (error (list (quote wrong-type-arg-to-builtin) print nil str-t)))
  (assert-eq test 5)
  )

(assert-eq (do (print nil) (print "should not be printed")) (error (list (quote wrong-type-arg-to-builtin) print nil str-t)))
