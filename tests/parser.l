(include "core/assert.l")

;; Lexer tests:

;; Expected separator.
(assert-eq (parse-lisp "12ab") (error '(lexer:expected-separator)))
(assert-eq (parse-lisp "0(") (error '(lexer:expected-separator)))
(assert-eq (parse-lisp "34\"") (error '(lexer:expected-separator)))

(assert-eq (parse-lisp "34no") (error '(lexer:expected-separator)))
(assert-eq (parse-lisp "69(") (error '(lexer:expected-separator)))
(assert-eq (parse-lisp "-0\"") (error '(lexer:expected-separator)))

(assert-eq (parse-lisp "abc(") (error '(lexer:expected-separator)))
(assert-eq (parse-lisp "abc\"") (error '(lexer:expected-separator)))

;; Unterminated string.
(assert-eq (parse-lisp "pass \"") (error '(lexer:unterminated-string)))
(assert-eq (parse-lisp "pass \"abc") (error '(lexer:unterminated-string)))
(assert-eq (parse-lisp "pass \"abc hello") (error '(lexer:unterminated-string)))
(assert-eq (parse-lisp "pass \"abc hello \n hi") (error '(lexer:unterminated-string)))
(assert-eq (parse-lisp "pass \"abc hello \\") (error '(lexer:unterminated-string)))
(assert-eq (parse-lisp "pass \"abc hello \\x") (error '(lexer:unterminated-string)))
(assert-eq (parse-lisp "pass \"abc hello \\x1") (error '(lexer:unterminated-string)))
(assert-eq (parse-lisp "pass \"abc hello \\xo") (error '(lexer:unterminated-string)))
(assert-eq (parse-lisp "pass \"abc hello \\n\\") (error '(lexer:unterminated-string)))

;; Unknown character.
(assert-eq (parse-lisp "\x01") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x02") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x03") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x04") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x05") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x06") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x07") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x08") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x0b") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x0c") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x0d") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x0e") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x0f") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x10") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x11") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x12") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x13") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x14") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x15") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x16") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x17") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x18") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x19") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x1a") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x1b") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x1c") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x1d") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x1e") (error '(lexer:unknown-char)))
(assert-eq (parse-lisp "\x1f") (error '(lexer:unknown-char)))

;; Invalid escape character or sequence.
(assert-eq (parse-lisp "pass \" \\c") (error '(lexer:invalid-escape)))
(assert-eq (parse-lisp "pass \" \\d") (error '(lexer:invalid-escape)))
(assert-eq (parse-lisp "pass \" \\p") (error '(lexer:invalid-escape)))
(assert-eq (parse-lisp "pass \" \\p") (error '(lexer:invalid-escape)))
(assert-eq (parse-lisp "pass \" \\\x05") (error '(lexer:invalid-escape)))
(assert-eq (parse-lisp "pass \" \\\x0a") (error '(lexer:invalid-escape)))
(assert-eq (parse-lisp "pass \" \\x0g") (error '(lexer:invalid-escape)))
(assert-eq (parse-lisp "pass \" \\xo4") (error '(lexer:invalid-escape)))


;; Parser tests:

;; Too few right parentheses check.
(assert-eq (parse-lisp "(a b c") (error '(parser:too-few-r-paren)))
(assert-eq (parse-lisp "(a (b c)") (error '(parser:too-few-r-paren)))
(assert-eq (parse-lisp "(a (b ( c") (error '(parser:too-few-r-paren)))
(assert-eq (parse-lisp "((a b) c") (error '(parser:too-few-r-paren)))
(assert-eq (parse-lisp "(a b) (c") (error '(parser:too-few-r-paren)))

;; Too many right parentheses check.
(assert-eq (parse-lisp "a b c)") (error '(parser:too-many-r-paren)))
(assert-eq (parse-lisp "(a b) c)") (error '(parser:too-many-r-paren)))
(assert-eq (parse-lisp ")a b) c)") (error '(parser:too-many-r-paren)))
(assert-eq (parse-lisp ")a b c") (error '(parser:too-many-r-paren)))
(assert-eq (parse-lisp "(a b)) c") (error '(parser:too-many-r-paren)))

(assert-eq (parse-lisp "a '") (error '(parser:reached-end-quote)))
(assert-eq (parse-lisp "a ''") (error '(parser:reached-end-quote)))
(assert-eq (parse-lisp "a '''") (error '(parser:reached-end-quote)))
(assert-eq (parse-lisp "a () '") (error '(parser:reached-end-quote)))
(assert-eq (parse-lisp "a () ''") (error '(parser:reached-end-quote)))
(assert-eq (parse-lisp "a () '''") (error '(parser:reached-end-quote)))
(assert-eq (parse-lisp "(a ')") (error '(parser:reached-end-quote)))
(assert-eq (parse-lisp "(a '')") (error '(parser:reached-end-quote)))
(assert-eq (parse-lisp "(a ''')") (error '(parser:reached-end-quote)))
(assert-eq (parse-lisp "(a () ')") (error '(parser:reached-end-quote)))
(assert-eq (parse-lisp "(a () '')") (error '(parser:reached-end-quote)))
(assert-eq (parse-lisp "(a () ''')") (error '(parser:reached-end-quote)))
