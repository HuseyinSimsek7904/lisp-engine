(include "core/assert.l")

;; Lexer tests:

;; Expected separator.
(assert-eq (parse-lisp "12ab") (error (quote (lexer:expected-separator))))
(assert-eq (parse-lisp "0(") (error (quote (lexer:expected-separator))))
(assert-eq (parse-lisp "34\"") (error (quote (lexer:expected-separator))))

(assert-eq (parse-lisp "34no") (error (quote (lexer:expected-separator))))
(assert-eq (parse-lisp "69(") (error (quote (lexer:expected-separator))))
(assert-eq (parse-lisp "-0\"") (error (quote (lexer:expected-separator))))

(assert-eq (parse-lisp "abc(") (error (quote (lexer:expected-separator))))
(assert-eq (parse-lisp "abc\"") (error (quote (lexer:expected-separator))))

;; Unterminated string.
(assert-eq (parse-lisp "pass \"") (error (quote (lexer:unterminated-string))))
(assert-eq (parse-lisp "pass \"abc") (error (quote (lexer:unterminated-string))))
(assert-eq (parse-lisp "pass \"abc hello") (error (quote (lexer:unterminated-string))))
(assert-eq (parse-lisp "pass \"abc hello \n hi") (error (quote (lexer:unterminated-string))))
(assert-eq (parse-lisp "pass \"abc hello \\") (error (quote (lexer:unterminated-string))))
(assert-eq (parse-lisp "pass \"abc hello \\x") (error (quote (lexer:unterminated-string))))
(assert-eq (parse-lisp "pass \"abc hello \\x1") (error (quote (lexer:unterminated-string))))
(assert-eq (parse-lisp "pass \"abc hello \\xo") (error (quote (lexer:unterminated-string))))
(assert-eq (parse-lisp "pass \"abc hello \\n\\") (error (quote (lexer:unterminated-string))))

;; Unknown character.
(assert-eq (parse-lisp "\x01") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x02") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x03") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x04") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x05") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x06") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x07") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x08") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x0b") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x0c") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x0d") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x0e") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x0f") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x10") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x11") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x12") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x13") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x14") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x15") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x16") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x17") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x18") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x19") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x1a") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x1b") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x1c") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x1d") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x1e") (error (quote (lexer:unknown-char))))
(assert-eq (parse-lisp "\x1f") (error (quote (lexer:unknown-char))))

;; Invalid escape character or sequence.
(assert-eq (parse-lisp "pass \" \\c") (error (quote (lexer:invalid-escape))))
(assert-eq (parse-lisp "pass \" \\d") (error (quote (lexer:invalid-escape))))
(assert-eq (parse-lisp "pass \" \\p") (error (quote (lexer:invalid-escape))))
(assert-eq (parse-lisp "pass \" \\p") (error (quote (lexer:invalid-escape))))
(assert-eq (parse-lisp "pass \" \\\x05") (error (quote (lexer:invalid-escape))))
(assert-eq (parse-lisp "pass \" \\\x0a") (error (quote (lexer:invalid-escape))))
(assert-eq (parse-lisp "pass \" \\x0g") (error (quote (lexer:invalid-escape))))
(assert-eq (parse-lisp "pass \" \\xo4") (error (quote (lexer:invalid-escape))))


;; Parser tests:

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
