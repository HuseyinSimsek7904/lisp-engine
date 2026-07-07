(include "core/assert.l")

;; Too few right parentheses check.
(assert-eq (parse-lisp "(a b c") (error (quote (parser:too-few-r-paren))))
(assert-eq (parse-lisp "(a (b c)") (error (quote (parser:too-few-r-paren))))
(assert-eq (parse-lisp "(a (b ( c") (error (quote (parser:too-few-r-paren))))
(assert-eq (parse-lisp "((a b) c") (error (quote (parser:too-few-r-paren))))
(assert-eq (parse-lisp "(a b) (c") (error (quote (parser:too-few-r-paren))))

;; Too many right parentheses check.
(assert-eq (parse-lisp "a b c)") (error (quote (parser:too-many-r-paren))))
(assert-eq (parse-lisp "(a b) c)") (error (quote (parser:too-many-r-paren))))
(assert-eq (parse-lisp ")a b) c)") (error (quote (parser:too-many-r-paren))))
(assert-eq (parse-lisp ")a b c") (error (quote (parser:too-many-r-paren))))
(assert-eq (parse-lisp "(a b)) c") (error (quote (parser:too-many-r-paren))))
