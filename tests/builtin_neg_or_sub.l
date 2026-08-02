(include "core/assert.l")

;; `-` structure: (- X [Y])

;; Wrong number of arguments.
(assert-eq (-)
           (error `(too-few-args-to-builtin ~- 0 1)))
(assert-eq (- 1 2 3)
           (error `(too-many-args-to-builtin ~- 3 2)))
(assert-eq (- 1 2 3 4)
           (error `(too-many-args-to-builtin ~- 4 2)))

;; Wrong type of arguments.
(assert-eq (- "abc")
           (error `(wrong-type-arg-to-builtin ~- "abc" ~int64-t)))
(assert-eq (- "abc" 1)
           (error `(wrong-type-arg-to-builtin ~- "abc" ~int64-t)))
(assert-eq (- 1 "abc")
           (error `(wrong-type-arg-to-builtin ~- "abc" ~int64-t)))

;; Error as argument.
(assert-eq (- (error "should not return") "anything")
           (error `(wrong-type-arg-to-builtin ~- ~(error "should not return") ~int64-t)))
(assert-eq (- "anything" (error "should not return"))
           (error `(wrong-type-arg-to-builtin ~- "anything" ~int64-t)))

;; Behavior tests.
(assert-eq (- 123) -123)
(assert-eq (- 12 23) -11)
