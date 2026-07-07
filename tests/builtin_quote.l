(include "core/assert.l")

;; `quote` structure: (quote VALUE)

;; Wrong number of arguments.
(assert-eq (quote) (error (list (quote wrong-number-of-args-to-builtin) quote 0 1)))
(assert-eq (quote 1 2) (error (list (quote wrong-number-of-args-to-builtin) quote 2 1)))

;; Functionality test.
(assert-eq (quote ()) nil)
(assert-eq (quote (1 2 3)) (list 1 2 3))
(assert-eq (list (quote a) (quote b) (quote c)) (quote (a b c)))
(assert-eq (list (quote a) "hello" 3) (quote (a "hello" 3)))
