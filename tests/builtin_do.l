(include "core/assert.l")

;; `do` structure: (do BODY...)

;; Wrong number of arguments.
(assert-eq (do) (error (list (quote too-few-args-to-builtin) do 0 1)))

;; If anything inside `BODY` returns error, it is returned immediately.
(assert-eq (do (print nil) (print "should not be printed")) (error (list (quote wrong-type-arg-to-builtin) print nil str-t)))
