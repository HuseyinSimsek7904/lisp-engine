(include "core/assert.l")

;; `if` structure: (if COND IF-TRUE [IF-FALSE...])

;; Wrong number of arguments.
(assert-eq (if)
           (error `(too-few-args-to-builtin ~if 0 2)))
(assert-eq (if 1)
           (error `(too-few-args-to-builtin ~if 1 2)))

;; Wrong type of argument to `COND` argument.
(assert-eq (if 1 2)
           (error `(wrong-type-arg-to-builtin ~if 1 ~bool-t)))

;; Error as argument.
(assert-eq (if (error "should return") "anything")
           (error "should return"))

;; `false` returns `nil` if `IF-FALSE` is empty.
(assert-eq (if false "anything") nil)
(assert-eq (if false anything) nil)

;; `true` returns `IF-TRUE`.
(assert-eq (if true "anything" (error "something")) "anything")
(assert-eq (if true anything (error "something"))
           (error '(undefined-symbol anything)))

;; `false` returns last in `IF-FALSE` unless anything else is error.
(assert-eq (if false "hello" "hi" "how are you") "how are you")
(assert-eq (if false "hello" (error "should return") "how are you")
           (error "should return"))
