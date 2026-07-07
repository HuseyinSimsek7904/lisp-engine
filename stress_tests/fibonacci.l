(include "examples/fibonacci.l")
(include "core/assert.l")

;; Result test.
(assert-eq (fib 25) 75025)
(assert-eq (fib 30) 832040)
