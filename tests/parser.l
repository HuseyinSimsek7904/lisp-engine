(setg assert-eq (fn (a b) (if (= a b) nil (error (list "expected equal" a b)))))

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
