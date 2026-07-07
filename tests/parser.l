(include "core/assert.l")

(assert-eq (parse-lisp "(a b c") (error (quote (parser:too-few-r-paren))))
(assert-eq (parse-lisp "(a (b c)") (error (quote (parser:too-few-r-paren))))
(assert-eq (parse-lisp "(a (b ( c") (error (quote (parser:too-few-r-paren))))
(assert-eq (parse-lisp "((a b) c") (error (quote (parser:too-few-r-paren))))
(assert-eq (parse-lisp "(a b) (c") (error (quote (parser:too-few-r-paren))))

(assert-eq (parse-lisp "a b c)") (error (quote (parser:too-many-r-paren))))
(assert-eq (parse-lisp "(a b) c)") (error (quote (parser:too-many-r-paren))))
(assert-eq (parse-lisp ")a b) c)") (error (quote (parser:too-many-r-paren))))
(assert-eq (parse-lisp ")a b c") (error (quote (parser:too-many-r-paren))))
(assert-eq (parse-lisp "(a b)) c") (error (quote (parser:too-many-r-paren))))
