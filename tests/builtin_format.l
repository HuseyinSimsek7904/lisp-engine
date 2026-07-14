(include "core/assert.l")

;; `format` structure: (format FORMAT [ARGS...])

;; Wrong number of arguments.
(assert-eq (format)
           (error (list 'too-few-args-to-builtin format 0 1)))

;; Wrong type of argument.
(assert-eq (format 1 nil)
           (error (list 'wrong-type-arg-to-builtin format 1 str-t)))

;; Too few args to format.
(assert-eq (format "abc %l def") (error (list 'too-few-args-to-format "abc %l def")))
(assert-eq (format "abc %s def") (error (list 'too-few-args-to-format "abc %s def")))
(assert-eq (format "abc %l def %l" 2) (error (list 'too-few-args-to-format "abc %l def %l")))

;; Wrong type arg to format.
(assert-eq (format "%s" 2) (error (list 'wrong-type-arg-to-format 2 str-t)))

;; Invalid format string.
(assert-eq (format "%d") (error (list 'invalid-format-string "%d")))

;; Functionality test.
(assert-eq (format "hello, %s" "world") "hello, world")
(assert-eq (format "%s, world" "hello") "hello, world")
(assert-eq (format "%s, %s" "hello" "world") "hello, world")
(assert-eq (format "%l, %l" "hello" "world") "\"hello\", \"world\"")

;; Test all formats.
(assert-eq (format "%l" nil) "<nil>")

(assert-eq (format "%l" true) "<true>")
(assert-eq (format "%l" false) "<false>")

(assert-eq (format "%l" int64-t) "<type `int64-t`>")
(assert-eq (format "%l" str-t) "<type `str-t`>")

(assert-eq (format "%l" (error 123)) "<error 123>")

(assert-eq (format "%l" 'abc) "abc")
(assert-eq (format "%l" 'str-t) "str-t")
(assert-eq (format "%l" 'str-t) "str-t")

(assert-eq (format "%l" -9223372036854775808) "-9223372036854775808")
(assert-eq (format "%l" -9223372036854775807) "-9223372036854775807")
(assert-eq (format "%l" -123) "-123")
(assert-eq (format "%l" -1) "-1")
(assert-eq (format "%l" 0) "0")
(assert-eq (format "%l" 1) "1")
(assert-eq (format "%l" 123) "123")
(assert-eq (format "%l" 9223372036854775807) "9223372036854775807")

(assert-eq (format "%l" format) "<builtin-fn `format`>")
(assert-eq (format "%l" print) "<builtin-fn `print`>")

(assert-eq (format "%l" (fn () nil)) "<fn <nil>>")
(assert-eq (format "%l" (fn (a) nil)) "<fn (a)>")
(assert-eq (format "%l" (fn (a b) nil)) "<fn (a b)>")
(assert-eq (format "%l" (fn (a b c) nil)) "<fn (a b c)>")
(assert-eq (format "%l" (fn ((rest r)) nil)) "<fn (r)>")
(assert-eq (format "%l" (fn (a (rest r)) nil)) "<fn (a r)>")
(assert-eq (format "%l" (fn (a b (rest r)) nil)) "<fn (a b r)>")
(assert-eq (format "%l" (fn (a b c (rest r)) nil)) "<fn (a b c r)>")

(assert-eq (format "%l" (list)) "<nil>")
(assert-eq (format "%l" (list 1)) "(1)")
(assert-eq (format "%l" (list 1 2)) "(1 2)")
(assert-eq (format "%l" (list 1 2 3)) "(1 2 3)")
(assert-eq (format "%l" '()) "<nil>")
(assert-eq (format "%l" '(a)) "(a)")
(assert-eq (format "%l" '(a b)) "(a b)")
(assert-eq (format "%l" '(a (b c) d)) "(a (b c) d)")

;; Testing unpacked formatting is not easy...
;; (assert-eq (format "%l" (unpack (list 1 2 3))) "<unpacked (1 2 3)>")

;; Test printable string formatting.
(assert-eq (format "%l" "hello") "\"hello\"")
(assert-eq (format "%l" "\a\b\t\n\v\r\e\"\'") "\"\\a\\b\\t\\n\\v\\r\\e\\\"\\\'\"")
(assert-eq (format "%l" "\x00\x1a\x12\x13") "\"\\x00\\x1a\\x12\\x13\"")
