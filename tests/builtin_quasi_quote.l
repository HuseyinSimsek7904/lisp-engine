(include "core/assert.l")

;; `quasi-quote` structure: (quote VALUE)

;; Wrong number of arguments.
(assert-eq (quasi-quote)
           (error `(wrong-number-of-args-to-builtin ~quasi-quote 0 1)))
(assert-eq (quasi-quote 1 2)
           (error `(wrong-number-of-args-to-builtin ~quasi-quote 2 1)))

;; Functionality test.
(assert-eq `() empty)
(assert-eq `(1 2 3 abc def "hello" "hi") '(1 2 3 abc def "hello" "hi"))
(assert-eq `(1 2 3 ~(list 4 5 6) 7 8 9) '(1 2 3 (4 5 6) 7 8 9))
(assert-eq `(1 2 3 ~@(list 4 5 6) 7 8 9) '(1 2 3 4 5 6 7 8 9))
(assert-eq `(1 2 3 ~((list 4 5 6)) 7 8 9) `(1 2 3 ~(error `(not-callable (4 5 6))) 7 8 9))
(assert-eq `(1 2 3 ~(+ 1 2 3) 7 8 9) '(1 2 3 6 7 8 9))
(assert-eq `(1 2 3 ~@(+ 1 2 3) 7 8 9) (error `(result-not-list (+ 1 2 3))))
