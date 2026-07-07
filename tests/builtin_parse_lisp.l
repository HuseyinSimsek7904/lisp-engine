(include "core/assert.l")

;; `parse-lisp` structure: (parse-lisp LISP)

;; Wrong number of arguments.
(assert-eq (parse-lisp) (error (list (quote wrong-number-of-args-to-builtin) parse-lisp 0 1)))
(assert-eq (parse-lisp 1 2) (error (list (quote wrong-number-of-args-to-builtin) parse-lisp 2 1)))

;; Wrong type of argument.
(assert-eq (parse-lisp 1) (error (list (quote wrong-type-arg-to-builtin) parse-lisp 1 str-t)))

;; Error as argument.
(assert-eq (parse-lisp (error "something")) (error (list (quote wrong-type-arg-to-builtin) parse-lisp (error "something") str-t)))

;; Testing of parsing is performed in tests/parser.l.
