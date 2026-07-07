(include "core/assert.l")

;; `unpack` structure: (unpack [LIST...])

;; Wrong type of argument.
(assert-eq (unpack (list 1 2 3) "abc") (error (list (quote wrong-type-arg-to-builtin) unpack "abc" list-t)))
(assert-eq (unpack "abc" (list 1 2 3)) (error (list (quote wrong-type-arg-to-builtin) unpack "abc" list-t)))

;; Error as argument.
(assert-eq (unpack (error "should not return") "anything") (error (list (quote wrong-type-arg-to-builtin) unpack (error "should not return") list-t)))
(assert-eq (unpack "anything" (error "should not return")) (error (list (quote wrong-type-arg-to-builtin) unpack "anything" list-t)))

;; Unpacking.
(assert-eq (+ (unpack)) 0)
(assert-eq (+ (unpack (list 1 2 3) (list 2 3 4))) 15)
