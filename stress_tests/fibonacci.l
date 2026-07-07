(include "examples/fibonacci.l")
(include "core/assert.l")

;; Result test.
(assert-eq (fib 25) (fib-linear 25))
(assert-eq (fib 30) (fib-linear 30))
