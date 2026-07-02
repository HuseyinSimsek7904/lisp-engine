(include "examples/fibonacci.l")

(setg assert-eq (fn (a b) (if (= a b) nil (list "expected equal" a b))))

(assert-eq (fib 25) 75025)
